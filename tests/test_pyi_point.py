#!/usr/bin/env python3
"""
Python unittest suite for the c104 package that exercises the full stub surface
of c104.Point and c104.Batch (as declared in c104/__init__.pyi).

IMPORTANT: this suite must be run with the INSTALLED c104 package (NOT the
in-tree c104/ source directory). Run it from a directory other than the repo
root, e.g.:

    cd /tmp && PYTHONUNBUFFERED=1 python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_point.py'

    # with coverage:
    cd /tmp && python3 -m coverage run --source c104 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_point.py' && python3 -m coverage report

Every test that opens real sockets uses a DISTINCT port to avoid TIME_WAIT
collisions and both server and client are stopped in tearDown.
"""

import datetime
import time
import unittest

import c104


def wait_for(predicate, timeout_s=10.0, interval_s=0.05):
    """Poll until predicate() is truthy or timeout elapses."""
    deadline = time.time() + timeout_s
    while time.time() < deadline:
        if predicate():
            return True
        time.sleep(interval_s)
    return False


_NEXT_PORT = [24301]


def _alloc_port():
    """Allocate a distinct port per test method to avoid TIME_WAIT reuse."""
    port = _NEXT_PORT[0]
    _NEXT_PORT[0] += 1
    return port


def _new_loopback(common_address):
    """
    Build and start a server<->client loopback on a fresh port, retrying until
    BOTH the client considers the connection open AND the server has registered
    an active connection (needed for interval based callbacks and server-side
    transmit). The library intermittently races the handshake in WSL, so the
    whole connection is recreated on a fresh port until a healthy one is up.

    Returns a dict with server/station/client/connection/cl_station or None.
    """
    for _ in range(8):
        port = _alloc_port()
        server = c104.Server(ip="127.0.0.1", port=port, tick_rate_ms=100)
        station = server.add_station(common_address=common_address)
        client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        connection = client.add_connection(ip="127.0.0.1", port=port,
                                           init=c104.Init.NONE)
        cl_station = connection.add_station(common_address=common_address)
        server.start()
        client.start()
        station.signal_initialized(cause=c104.Coi.LOCAL_MANUAL_RESET)
        connected = wait_for(lambda: connection.is_connected, timeout_s=3.0)
        active = wait_for(lambda: server.has_active_connections,
                          timeout_s=3.0)
        if connected and active:
            return {
                "server": server, "station": station, "client": client,
                "connection": connection, "cl_station": cl_station,
            }
        try:
            client.stop()
        except Exception:
            pass
        try:
            server.stop()
        except Exception:
            pass
    return None


def _stop_loopback(objs):
    if not objs:
        return
    try:
        objs["client"].stop()
    except Exception:
        pass
    try:
        objs["server"].stop()
    except Exception:
        pass


class PointServerTestCase(unittest.TestCase):
    """
    Server-backed points attached to a running loopback so that the interval
    based callbacks (on_timer / on_before_auto_transmit) and command based
    callbacks (on_receive / on_before_read) are driven by real traffic.
    """

    PORT = 24301

    def setUp(self):
        self.loop = _new_loopback(common_address=47)
        self.assertIsNotNone(self.loop, "could not establish a healthy loopback")
        self.port = self.loop["connection"].port
        self.server = self.loop["server"]
        self.station = self.loop["station"]
        self.client = self.loop["client"]
        self.connection = self.loop["connection"]
        self.cl_station = self.loop["cl_station"]

    def tearDown(self):
        _stop_loopback(getattr(self, "loop", None))
        self.server = None
        self.station = None
        self.client = None
        self.connection = None
        self.cl_station = None
        self.loop = None

    def test_point_identity_and_basic_getters(self):
        p = self.station.add_point(io_address=11, type=c104.Type.M_ME_NC_1)
        self.assertIsNotNone(p)
        self.assertIsInstance(p, c104.Point)
        self.assertEqual(p.io_address, 11)
        self.assertEqual(p.type, c104.Type.M_ME_NC_1)
        self.assertIs(p.station, self.station)
        self.assertIsInstance(p.processed_at, datetime.datetime)
        self.assertIsNone(p.recorded_at)
        self.assertIsNone(p.selected_by)
        self.assertEqual(p.timer_ms, 0)

    def test_value_float_short_measurement(self):
        p = self.station.add_point(io_address=12, type=c104.Type.M_ME_NC_1)
        p.value = 1234.5
        self.assertAlmostEqual(p.value, 1234.5, places=3)
        self.assertAlmostEqual(p.info.value, 1234.5, places=3)

    def test_value_bool_single_point(self):
        p = self.station.add_point(io_address=13, type=c104.Type.M_SP_NA_1)
        p.value = True
        self.assertEqual(p.value, True)
        p.value = False
        self.assertEqual(p.value, False)

    def test_value_double_point(self):
        p = self.station.add_point(io_address=14, type=c104.Type.M_DP_TB_1)
        p.value = c104.Double.ON
        self.assertEqual(p.value, c104.Double.ON)
        p.value = c104.Double.OFF
        self.assertEqual(p.value, c104.Double.OFF)

    def test_value_step_point(self):
        p = self.station.add_point(io_address=15, type=c104.Type.M_ST_TB_1)
        p.value = c104.Int7(42)
        self.assertIsNotNone(p.value)

    def test_value_type_mismatch_raises(self):
        p = self.station.add_point(io_address=16, type=c104.Type.M_ME_NC_1)
        p.value = 12.0
        with self.assertRaises(ValueError):
            p.value = True  # bool not allowed for a float point

    def test_info_set_get(self):
        p = self.station.add_point(io_address=17, type=c104.Type.M_ME_NC_1)
        new_info = c104.ShortInfo(actual=77.7, quality=c104.Quality.Invalid,
                                  recorded_at=datetime.datetime.now(
                                      datetime.timezone.utc))
        p.info = new_info
        self.assertAlmostEqual(p.info.value, 77.7, places=3)
        self.assertEqual(p.info.quality, c104.Quality.Invalid)
        # SECURITY.md L12: the point stores a private copy, the caller's object
        # is not modified (M_ME_NC_1 drops the timestamp only in the copy) and
        # points assigned the same object do not share state
        self.assertIsNot(p.info, new_info)
        self.assertIsNone(p.info.recorded_at)
        self.assertIsNotNone(new_info.recorded_at)
        p2 = self.station.add_point(io_address=117, type=c104.Type.M_ME_NC_1)
        p2.info = new_info
        p.value = 1.5
        self.assertAlmostEqual(p.value, 1.5, places=3)
        self.assertAlmostEqual(p2.value, 77.7, places=3)
        self.assertAlmostEqual(new_info.value, 77.7, places=3)

    def test_quality_set_get(self):
        p = self.station.add_point(io_address=18, type=c104.Type.M_ME_NC_1)
        p.value = 1.0
        p.quality = c104.Quality.Invalid | c104.Quality.Overflow
        self.assertTrue(p.quality.is_any())
        self.assertFalse(p.quality.is_good())
        self.assertIsNotNone(p.quality.value)

    def test_command_mode_set_get(self):
        p = self.station.add_point(io_address=19, type=c104.Type.C_SC_NA_1)
        p.command_mode = c104.CommandMode.SELECT_AND_EXECUTE
        self.assertEqual(p.command_mode, c104.CommandMode.SELECT_AND_EXECUTE)
        p.command_mode = c104.CommandMode.DIRECT
        self.assertEqual(p.command_mode, c104.CommandMode.DIRECT)

    def test_report_ms_set_get(self):
        p = self.station.add_point(io_address=20, type=c104.Type.M_ME_NC_1)
        p.report_ms = 200
        self.assertEqual(p.report_ms, 200)
        p.report_ms = 0
        self.assertEqual(p.report_ms, 0)
        with self.assertRaises(ValueError):
            p.report_ms = 150  # not an integer multiple of tick_rate_ms (100)

    def test_related_io_address_and_autoreturn(self):
        self.station.add_point(io_address=21, type=c104.Type.M_ME_NC_1)
        c_point = self.station.add_point(io_address=22,
                                         type=c104.Type.C_SC_NA_1,
                                         related_io_address=21,
                                         related_io_autoreturn=True)
        self.assertEqual(c_point.related_io_address, 21)
        self.assertTrue(c_point.related_io_autoreturn)
        c_point.related_io_address = None
        # "no related point" is represented as None (or the 0 sentinel)
        self.assertFalse(c_point.related_io_address)
        c_point.related_io_autoreturn = False
        self.assertFalse(c_point.related_io_autoreturn)

    def test_transmit_server_side(self):
        p = self.station.add_point(io_address=23, type=c104.Type.M_ME_NC_1)
        p.value = 5.0
        # server transmit only succeeds while the server has an active
        # connection, so retry until it registers.
        self.assertTrue(wait_for(lambda: p.transmit(
            cause=c104.Cot.SPONTANEOUS) is True, timeout_s=5.0),
            "server-side transmit never succeeded")
        p.transmit(cause=c104.Cot.SPONTANEOUS, originator=1)

    def test_timer_callback_fires(self):
        p = self.station.add_point(io_address=24, type=c104.Type.M_ME_NC_1)
        fired = []

        def on_timer(point: c104.Point) -> None:
            fired.append(point.io_address)

        p.on_timer(callable=on_timer, interval_ms=200)
        self.assertEqual(p.timer_ms, 200)
        self.assertTrue(wait_for(lambda: len(fired) >= 1, timeout_s=10.0),
                        "on_timer callback never fired")

    def test_before_auto_transmit_callback_fires(self):
        p = self.station.add_point(io_address=25, type=c104.Type.M_ME_NC_1)
        p.value = 1.0
        fired = []

        def on_before(point: c104.Point) -> None:
            fired.append(point.io_address)

        p.on_before_auto_transmit(callable=on_before)
        p.report_ms = 300
        self.assertTrue(wait_for(lambda: len(fired) >= 1, timeout_s=10.0),
                        "on_before_auto_transmit callback never fired")

    def test_on_before_read_callback_fires(self):
        # on_before_read fires when a connected client issues an interrogation.
        sv_p = self.station.add_point(io_address=30, type=c104.Type.M_ME_NC_1)
        sv_p.value = 2.5
        self.cl_station.add_point(io_address=30, type=c104.Type.M_ME_NC_1)
        fired = []

        def on_before(point: c104.Point) -> None:
            fired.append(point.io_address)

        sv_p.on_before_read(callable=on_before)
        self.assertTrue(
            self.connection.interrogation(common_address=47,
                                          cause=c104.Cot.ACTIVATION,
                                          qualifier=c104.Qoi.STATION))
        self.assertTrue(wait_for(lambda: len(fired) >= 1, timeout_s=5.0),
                        "on_before_read callback never fired")

    def test_on_receive_callback_fires(self):
        # client sends a single command; server command point on_receive fires
        # and returns SUCCESS.
        cmd_point = self.station.add_point(io_address=40,
                                           type=c104.Type.C_SC_NA_1)
        fired = []

        def on_rcv(point: c104.Point, previous_info: c104.Information,
                   message: c104.IncomingMessage) -> c104.ResponseState:
            fired.append(point.io_address)
            return c104.ResponseState.SUCCESS

        cmd_point.on_receive(callable=on_rcv)
        cl_cmd = self.cl_station.add_point(io_address=40,
                                           type=c104.Type.C_SC_NA_1)
        cl_cmd.value = True
        self.assertTrue(wait_for(lambda: cl_cmd.transmit(
            cause=c104.Cot.ACTIVATION) is True, timeout_s=5.0))
        self.assertTrue(wait_for(lambda: len(fired) >= 1, timeout_s=5.0),
                        "on_receive callback never fired")


class PointClientTestCase(unittest.TestCase):
    """Client-backed point: read()/transmit() and receive round trips."""

    PORT = 24304

    def setUp(self):
        self.loop = _new_loopback(common_address=80)
        self.assertIsNotNone(self.loop, "could not establish a healthy loopback")
        self.server = self.loop["server"]
        self.sv_station = self.loop["station"]
        self.client = self.loop["client"]
        self.connection = self.loop["connection"]
        self.cl_station = self.loop["cl_station"]

    def tearDown(self):
        _stop_loopback(getattr(self, "loop", None))
        self.server = None
        self.sv_station = None
        self.client = None
        self.connection = None
        self.cl_station = None
        self.loop = None

    def test_client_read_method_invocation(self):
        # point.read() drives a real protocol read command; it must be callable
        # and return a bool without raising.
        sv_p = self.sv_station.add_point(io_address=50,
                                         type=c104.Type.M_ME_NC_1)
        sv_p.value = 3.25
        cl_p = self.cl_station.add_point(io_address=50,
                                         type=c104.Type.M_ME_NC_1)
        result = cl_p.read()
        self.assertIsInstance(result, bool)

    def test_client_measurement_receive(self):
        # server transmits spontaneous measurement -> client point gets value
        sv_p = self.sv_station.add_point(io_address=51,
                                         type=c104.Type.M_SP_NA_1)
        sv_p.value = True
        cl_p = self.cl_station.add_point(io_address=51,
                                         type=c104.Type.M_SP_NA_1)
        self.assertTrue(wait_for(lambda: sv_p.transmit(
            cause=c104.Cot.SPONTANEOUS) is True, timeout_s=5.0))
        self.assertTrue(wait_for(lambda: cl_p.value is True, timeout_s=5.0),
                        "client point never received the value")

    def test_client_command_transmit(self):
        self.sv_station.add_point(io_address=52, type=c104.Type.C_SC_NA_1)
        cl_cmd = self.cl_station.add_point(io_address=52,
                                           type=c104.Type.C_SC_NA_1)
        cl_cmd.value = True
        self.assertTrue(wait_for(lambda: cl_cmd.transmit(
            cause=c104.Cot.ACTIVATION) is True, timeout_s=5.0))


class BatchTestCase(unittest.TestCase):
    """Batch: construction, add_point, and all getters."""

    PORT = 24305

    def setUp(self):
        port = _alloc_port()
        self.server = c104.Server(ip="127.0.0.1", port=port,
                                  tick_rate_ms=100)
        self.station = self.server.add_station(common_address=90)
        self.server.start()

    def tearDown(self):
        try:
            self.server.stop()
        except Exception:
            pass
        self.server = None
        self.station = None

    def test_batch_construction_and_getters(self):
        p1 = self.station.add_point(io_address=1, type=c104.Type.M_ME_NC_1)
        p2 = self.station.add_point(io_address=2, type=c104.Type.M_ME_NC_1)
        p3 = self.station.add_point(io_address=3, type=c104.Type.M_ME_NC_1)
        batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[p1, p2, p3])
        self.assertTrue(batch.has_points)
        self.assertEqual(batch.number_of_objects, 3)
        self.assertEqual(batch.common_address, 90)
        self.assertEqual(batch.cot, c104.Cot.SPONTANEOUS)
        self.assertEqual(batch.type, c104.Type.M_ME_NC_1)
        self.assertFalse(batch.is_negative)
        self.assertFalse(batch.is_test)
        self.assertEqual(batch.originator_address, 0)
        self.assertEqual(len(batch.points), 3)
        self.assertIn(p1, batch.points)
        self.assertIn(p3, batch.points)

    def test_batch_empty(self):
        batch = c104.Batch(cause=c104.Cot.PERIODIC)
        self.assertFalse(batch.has_points)
        self.assertEqual(batch.number_of_objects, 0)
        self.assertEqual(batch.points, [])

    def test_batch_add_point(self):
        p1 = self.station.add_point(io_address=5, type=c104.Type.M_ME_NC_1)
        p2 = self.station.add_point(io_address=6, type=c104.Type.M_ME_NC_1)
        batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[p1])
        batch.add_point(p2)
        self.assertEqual(batch.number_of_objects, 2)
        self.assertIn(p2, batch.points)

    def test_batch_is_sequence(self):
        # consecutive io_addresses of the same single-point type -> sequence
        pts = []
        for i in range(3):
            pts.append(self.station.add_point(
                io_address=10 + i, type=c104.Type.M_SP_NA_1))
        batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=pts)
        self.assertTrue(batch.is_sequence)
        self.assertEqual(batch.type, c104.Type.M_SP_NA_1)

    def test_batch_incompatible_points_raises(self):
        other = self.server.add_station(common_address=99)
        p1 = self.station.add_point(io_address=20, type=c104.Type.M_SP_NA_1)
        p2 = other.add_point(io_address=21, type=c104.Type.M_SP_NA_1)
        with self.assertRaises(ValueError):
            c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[p1, p2])

    def test_batch_add_duplicate_raises(self):
        p = self.station.add_point(io_address=30, type=c104.Type.M_SP_NA_1)
        batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[p])
        with self.assertRaises(ValueError):
            batch.add_point(p)


if __name__ == "__main__":
    unittest.main()
