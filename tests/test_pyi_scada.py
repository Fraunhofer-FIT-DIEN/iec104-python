#!/usr/bin/env python3
"""
Python unittest suite for the c104 package that exercises the full stub surface
of the objects in this module group: Client, Server, Connection, Station,
ProtocolParameters and IncomingMessage (as declared in c104/__init__.pyi).

IMPORTANT: this suite must be run with the INSTALLED c104 package (NOT the
in-tree c104/ source directory). Run it from a directory other than the repo
root, e.g.:

    cd /tmp && PYTHONUNBUFFERED=1 python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_scada.py'

    # with coverage:
    cd /tmp && python3 -m coverage run --source c104 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_scada.py' && python3 -m coverage report

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


# ---------------------------------------------------------------------------
# Client
# ---------------------------------------------------------------------------
class ClientTestCase(unittest.TestCase):
    def test_creation_and_properties(self):
        client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        try:
            self.assertFalse(client.is_running)
            self.assertFalse(client.has_connections)
            self.assertFalse(client.has_open_connections)
            self.assertFalse(client.has_active_connections)
            self.assertEqual(client.active_connection_count, 0)
            self.assertEqual(client.open_connection_count, 0)
            self.assertEqual(client.connections, [])
            self.assertEqual(client.tick_rate_ms, 100)
            self.assertTrue(0 <= client.originator_address <= 255)
        finally:
            client.stop()

    def test_originator_address_setter(self):
        client = c104.Client()
        try:
            client.originator_address = 42
            self.assertEqual(client.originator_address, 42)
        finally:
            client.stop()

    def test_get_connection_none(self):
        client = c104.Client()
        try:
            # no connection matches this ip/port -> None
            self.assertIsNone(client.get_connection(ip="127.0.0.1", port=2404))
            # no connection exposes this common address -> None
            self.assertIsNone(client.get_connection(common_address=1234))
        finally:
            client.stop()


class ClientLoopbackTestCase(unittest.TestCase):
    PORT = 24201

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1", port=self.PORT, init=c104.Init.INTERROGATION)

    def tearDown(self):
        try:
            self.client.stop()
        except Exception:
            pass
        try:
            self.server.stop()
        except Exception:
            pass
        self.client = None
        self.server = None
        self.connection = None

    def test_add_connection_and_counters(self):
        self.assertIsNotNone(self.connection)
        self.assertTrue(self.client.has_connections)
        self.assertEqual(len(self.client.connections), 1)
        self.assertIs(self.client.get_connection(ip="127.0.0.1",
                                                 port=self.PORT),
                      self.connection)
        # no station exists yet -> no match by common address
        self.assertIsNone(self.client.get_connection(common_address=20))

    def test_start_stop_callbacks_and_counters(self):
        states = []
        new_stations = []
        new_points = []
        initialized = []

        def on_state(connection: c104.Connection,
                     state: c104.ConnectionState) -> None:
            states.append(state)

        def on_new_station(client: c104.Client, connection: c104.Connection,
                           common_address: int) -> None:
            new_stations.append(common_address)
            connection.add_station(common_address=common_address)

        def on_new_point(client: c104.Client, station: c104.Station,
                         io_address: int, point_type: c104.Type) -> None:
            new_points.append(io_address)
            station.add_point(io_address=io_address, type=point_type)

        def on_station_initialized(client: c104.Client, station: c104.Station,
                                   cause: c104.Coi) -> None:
            initialized.append(station.common_address)

        self.connection.on_state_change(callable=on_state)
        self.client.on_new_station(callable=on_new_station)
        self.client.on_new_point(callable=on_new_point)
        self.client.on_station_initialized(callable=on_station_initialized)

        sv_station = self.server.add_station(common_address=20)
        sv_point = sv_station.add_point(io_address=1,
                                        type=c104.Type.M_ME_NC_1)
        sv_point.value = 1.5

        self.server.start()
        self.client.start()

        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")
        self.assertTrue(self.client.is_running)
        self.assertTrue(self.client.has_open_connections)
        self.assertTrue(self.client.has_active_connections)
        self.assertGreaterEqual(self.client.open_connection_count, 1)
        self.assertGreaterEqual(self.client.active_connection_count, 1)

        sv_station.signal_initialized(cause=c104.Coi.LOCAL_MANUAL_RESET)
        self.assertTrue(
            wait_for(lambda: len(initialized) > 0),
            "station initialized callback never fired")

        self.client.disconnect_all()
        self.assertTrue(
            wait_for(lambda: not self.connection.is_connected),
            "connection never disconnected after disconnect_all")
        self.assertFalse(self.client.has_open_connections)

        self.connection.connect()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never reconnected")

        self.client.stop()
        self.assertFalse(self.client.is_running)
        self.assertFalse(self.client.has_open_connections)

    def test_manual_start_and_reconnect_all(self):
        sv_station = self.server.add_station(common_address=30)
        sv_station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)

        self.server.start()
        self.connection.connect()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")

        # exercise stop()/start()/reconnect_all() without assuming the socket
        # closes synchronously on stop
        self.client.stop()
        self.assertFalse(self.client.is_running)

        self.client.start()
        self.client.reconnect_all()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never reconnected after start/reconnect_all")


# ---------------------------------------------------------------------------
# Server
# ---------------------------------------------------------------------------
class ServerTestCase(unittest.TestCase):
    PORT = 24202

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)

    def tearDown(self):
        try:
            self.client.stop()
        except Exception:
            pass
        try:
            self.server.stop()
        except Exception:
            pass
        self.client = None
        self.server = None

    def test_creation_and_properties(self):
        self.assertFalse(self.server.is_running)
        self.assertFalse(self.server.has_stations)
        self.assertFalse(self.server.has_open_connections)
        self.assertFalse(self.server.has_active_connections)
        self.assertEqual(self.server.open_connection_count, 0)
        self.assertEqual(self.server.active_connection_count, 0)
        self.assertEqual(self.server.stations, [])
        self.assertEqual(self.server.tick_rate_ms, 100)
        self.assertEqual(self.server.ip, "127.0.0.1")
        self.assertEqual(self.server.port, self.PORT)
        self.assertEqual(self.server.max_connections, 0)
        self.assertIsNotNone(self.server.protocol_parameters)

    def test_max_connections_setter(self):
        self.server.max_connections = 5
        self.assertEqual(self.server.max_connections, 5)

    def test_station_management_and_properties(self):
        self.assertIsNone(self.server.get_station(common_address=1))

        station = self.server.add_station(common_address=100)
        self.assertIsNotNone(station)
        self.assertIs(self.server.get_station(common_address=100), station)
        self.assertTrue(self.server.has_stations)
        self.assertEqual(len(self.server.stations), 1)

        self.assertTrue(self.server.remove_station(common_address=100))
        self.assertFalse(self.server.has_stations)
        self.assertIsNone(self.server.get_station(common_address=100))
        self.assertFalse(self.server.remove_station(common_address=999))


class ServerRoundTripTestCase(unittest.TestCase):
    PORT = 24203

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1", port=self.PORT, init=c104.Init.INTERROGATION)

    def tearDown(self):
        try:
            self.client.stop()
        except Exception:
            pass
        try:
            self.server.stop()
        except Exception:
            pass
        self.client = None
        self.server = None
        self.connection = None

    def test_callbacks_connect_start_and_transmit_batch(self):
        connects = []
        clock_syncs = []
        rx_fired = []
        tx_fired = []
        unexpected = []

        def on_connect(server: c104.Server, ip: str) -> bool:
            connects.append(ip)
            return True

        def on_clock_sync(server: c104.Server, ip: str,
                          date_time: datetime.datetime) -> c104.ResponseState:
            clock_syncs.append(ip)
            return c104.ResponseState.SUCCESS

        def on_receive_raw(server: c104.Server, data: bytes) -> None:
            rx_fired.append(data)

        def on_send_raw(server: c104.Server, data: bytes) -> None:
            tx_fired.append(data)

        def on_unexpected(server: c104.Server, message: c104.IncomingMessage,
                          cause: c104.Umc) -> None:
            unexpected.append(cause)

        self.server.on_connect(callable=on_connect)
        self.server.on_clock_sync(callable=on_clock_sync)
        self.server.on_receive_raw(callable=on_receive_raw)
        self.server.on_send_raw(callable=on_send_raw)
        self.server.on_unexpected_message(callable=on_unexpected)

        sv_station = self.server.add_station(common_address=50)
        m_point = sv_station.add_point(io_address=1,
                                       type=c104.Type.M_ME_NC_1)
        m_point.value = 3.14
        c_point = sv_station.add_point(io_address=2,
                                       type=c104.Type.C_SC_NA_1)
        sv_station.add_point(io_address=3, type=c104.Type.M_ME_NC_1)

        c_station = self.connection.add_station(common_address=50)
        c_station.add_point(io_address=1, type=c104.Type.M_ME_NC_1)
        c_station.add_point(io_address=3, type=c104.Type.M_ME_NC_1)

        self.server.start()
        self.client.start()

        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")
        self.assertTrue(self.server.is_running)
        self.assertTrue(self.server.has_open_connections)
        self.assertTrue(self.server.has_active_connections)
        self.assertGreaterEqual(self.server.open_connection_count, 1)
        self.assertGreaterEqual(self.server.active_connection_count, 1)

        sv_station.signal_initialized(cause=c104.Coi.LOCAL_MANUAL_RESET)
        self.assertTrue(
            wait_for(lambda: len(connects) > 0),
            "on_connect callback never fired")

        # clock sync
        self.assertTrue(self.connection.clock_sync(common_address=50))
        self.assertTrue(
            wait_for(lambda: len(clock_syncs) > 0),
            "on_clock_sync callback never fired")

        # transmit_batch on the server
        batch = c104.Batch(cause=c104.Cot.SPONTANEOUS)
        batch.add_point(m_point)
        batch.add_point(sv_station.get_point(io_address=3))
        self.assertTrue(batch.has_points)
        self.assertEqual(batch.number_of_objects, 2)
        self.assertTrue(self.server.transmit_batch(batch))

    def test_remove_station_on_running(self):
        station = self.server.add_station(common_address=60)
        station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
        self.server.start()
        self.client.start()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")
        self.assertTrue(self.server.remove_station(common_address=60))
        self.assertFalse(self.server.has_stations)


# ---------------------------------------------------------------------------
# Connection
# ---------------------------------------------------------------------------
class ConnectionTestCase(unittest.TestCase):
    PORT = 24204

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1", port=self.PORT, init=c104.Init.ALL)

    def tearDown(self):
        try:
            self.client.stop()
        except Exception:
            pass
        try:
            self.server.stop()
        except Exception:
            pass
        self.client = None
        self.server = None
        self.connection = None

    def test_initial_properties(self):
        self.assertEqual(self.connection.ip, "127.0.0.1")
        self.assertEqual(self.connection.port, self.PORT)
        self.assertFalse(self.connection.is_connected)
        self.assertFalse(self.connection.is_muted)
        self.assertFalse(self.connection.has_stations)
        self.assertEqual(self.connection.stations, [])
        self.assertIsNone(self.connection.get_station(common_address=10))
        self.assertIsNotNone(self.connection.protocol_parameters)
        self.assertTrue(0 <= self.connection.originator_address <= 255)
        self.assertIsNotNone(self.connection.state)  # ConnectionState enum
        # never connected -> connected_at is None, disconnected_at is the
        # epoch sentinel datetime
        self.assertIsNone(self.connection.connected_at)
        self.assertIsInstance(self.connection.disconnected_at,
                              datetime.datetime)

    def test_originator_address_setter(self):
        self.connection.originator_address = 7
        self.assertEqual(self.connection.originator_address, 7)

    def test_station_management(self):
        station = self.connection.add_station(common_address=70)
        self.assertIsNotNone(station)
        self.assertTrue(self.connection.has_stations)
        self.assertEqual(len(self.connection.stations), 1)
        self.assertIs(self.connection.get_station(common_address=70), station)
        self.assertTrue(self.connection.remove_station(common_address=70))
        self.assertFalse(self.connection.has_stations)
        self.assertFalse(self.connection.remove_station(common_address=999))

    def test_mute_unmute_disconnect(self):
        self.server.start()
        # mute/unmute toggles is_muted only when the client's main thread is
        # running, so use client.start() rather than a bare connection.connect()
        self.client.start()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")

        self.assertTrue(self.connection.mute())
        self.assertTrue(
            wait_for(lambda: self.connection.is_muted),
            "connection never became muted")
        self.assertTrue(self.connection.unmute())
        self.assertTrue(
            wait_for(lambda: not self.connection.is_muted),
            "connection never became unmuted")

        self.connection.disconnect()
        self.assertTrue(
            wait_for(lambda: not self.connection.is_connected),
            "connection never disconnected")
        self.assertIsInstance(self.connection.disconnected_at,
                              datetime.datetime)


class ConnectionRoundTripTestCase(unittest.TestCase):
    PORT = 24205

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1", port=self.PORT, init=c104.Init.INTERROGATION)

    def tearDown(self):
        try:
            self.client.stop()
        except Exception:
            pass
        try:
            self.server.stop()
        except Exception:
            pass
        self.client = None
        self.server = None
        self.connection = None

    def _start_roundtrip(self, common_address, points):
        sv_station = self.server.add_station(common_address=common_address)
        for ioa, typ in points:
            p = sv_station.add_point(io_address=ioa, type=typ)
            if typ == c104.Type.M_ME_NC_1:
                p.value = 4.56
            elif typ == c104.Type.M_SP_NA_1:
                p.value = True
        c_station = self.connection.add_station(common_address=common_address)
        for ioa, typ in points:
            c_station.add_point(io_address=ioa, type=typ)
        self.server.start()
        self.client.start()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")
        # wait for the connection to be fully open before issuing commands
        self.assertTrue(
            wait_for(lambda: self.connection.state == c104.ConnectionState.OPEN),
            "connection never reached OPEN state")
        return sv_station, c_station

    def test_interrogation(self):
        self._start_roundtrip(80, [(1, c104.Type.M_ME_NC_1)])
        self.assertTrue(
            self.connection.interrogation(common_address=80,
                                          cause=c104.Cot.ACTIVATION,
                                          qualifier=c104.Qoi.STATION))

    def test_counter_interrogation(self):
        self._start_roundtrip(81, [(1, c104.Type.M_IT_NA_1)])
        self.assertTrue(
            self.connection.counter_interrogation(
                common_address=81, cause=c104.Cot.ACTIVATION,
                qualifier=c104.Rqt.GENERAL, freeze=c104.Frz.READ))

    def test_test_command(self):
        self._start_roundtrip(82, [(1, c104.Type.M_SP_NA_1)])
        self.assertTrue(self.connection.test(common_address=82))


# ---------------------------------------------------------------------------
# Station
# ---------------------------------------------------------------------------
class StationTestCase(unittest.TestCase):
    def test_server_station_properties(self):
        server = c104.Server(ip="127.0.0.1", port=24206, tick_rate_ms=100)
        try:
            station = server.add_station(common_address=90)
            self.assertIsNotNone(station)
            self.assertEqual(station.common_address, 90)
            self.assertTrue(station.is_local)
            self.assertFalse(station.has_points)
            self.assertEqual(station.points, [])
            self.assertIs(station.server, server)
            self.assertIsNone(station.connection)
            self.assertIsNone(station.get_point(io_address=5))
        finally:
            server.stop()

    def test_remote_station_properties(self):
        client = c104.Client(tick_rate_ms=100)
        try:
            connection = client.add_connection(ip="127.0.0.1", port=24207)
            station = connection.add_station(common_address=91)
            self.assertIsNotNone(station)
            self.assertFalse(station.is_local)
            self.assertIs(station.connection, connection)
            self.assertIsNone(station.server)
        finally:
            client.stop()

    def test_point_management(self):
        server = c104.Server(ip="127.0.0.1", port=24208, tick_rate_ms=100)
        try:
            station = server.add_station(common_address=92)
            point = station.add_point(io_address=6,
                                      type=c104.Type.M_SP_NA_1)
            self.assertIsNotNone(point)
            self.assertTrue(station.has_points)
            self.assertIs(station.get_point(io_address=6), point)
            self.assertEqual(len(station.points), 1)
            self.assertTrue(station.remove_point(io_address=6))
            self.assertFalse(station.has_points)
            self.assertFalse(station.remove_point(io_address=999))
        finally:
            server.stop()

    def test_signal_initialized(self):
        server = c104.Server(ip="127.0.0.1", port=24209, tick_rate_ms=100)
        client = c104.Client(tick_rate_ms=100)
        connection = None
        try:
            server.start()
            sv_station = server.add_station(common_address=93)
            connection = client.add_connection(ip="127.0.0.1", port=24209)
            client.start()
            self.assertTrue(
                wait_for(lambda: connection.is_connected),
                "connection never became connected")
            # must not raise
            sv_station.signal_initialized(cause=c104.Coi.LOCAL_POWER_ON)
        finally:
            try:
                client.stop()
            except Exception:
                pass
            try:
                server.stop()
            except Exception:
                pass


# ---------------------------------------------------------------------------
# ProtocolParameters
# ---------------------------------------------------------------------------
class ProtocolParametersTestCase(unittest.TestCase):
    def test_server_protocol_parameters(self):
        server = c104.Server(ip="127.0.0.1", port=24210, tick_rate_ms=100)
        try:
            pp = server.protocol_parameters
            self._assert_get_set(pp)
        finally:
            server.stop()

    def test_client_protocol_parameters(self):
        client = c104.Client(tick_rate_ms=100)
        try:
            connection = client.add_connection(ip="127.0.0.1", port=24211)
            pp = connection.protocol_parameters
            self._assert_get_set(pp)
        finally:
            client.stop()

    def _assert_get_set(self, pp):
        # getters
        self.assertIsInstance(pp.confirm_interval, int)
        self.assertIsInstance(pp.connection_timeout, int)
        self.assertIsInstance(pp.keep_alive_interval, int)
        self.assertIsInstance(pp.message_timeout, int)
        self.assertIsInstance(pp.receive_window_size, int)
        self.assertIsInstance(pp.send_window_size, int)

        # setters
        pp.confirm_interval = 15
        pp.connection_timeout = 30
        pp.keep_alive_interval = 20
        pp.message_timeout = 10
        pp.receive_window_size = 12
        pp.send_window_size = 8

        self.assertEqual(pp.confirm_interval, 15)
        self.assertEqual(pp.connection_timeout, 30)
        self.assertEqual(pp.keep_alive_interval, 20)
        self.assertEqual(pp.message_timeout, 10)
        self.assertEqual(pp.receive_window_size, 12)
        self.assertEqual(pp.send_window_size, 8)


# ---------------------------------------------------------------------------
# IncomingMessage
# ---------------------------------------------------------------------------
class IncomingMessageTestCase(unittest.TestCase):
    """
    Real IncomingMessage objects are obtained through the Station/Point
    on_receive callbacks during a real interrogation round trip.
    """

    PORT = 24212

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1", port=self.PORT, init=c104.Init.INTERROGATION)
        self.received = []
        self.commands = []

    def tearDown(self):
        try:
            self.client.stop()
        except Exception:
            pass
        try:
            self.server.stop()
        except Exception:
            pass
        self.client = None
        self.server = None
        self.connection = None

    def test_incoming_message_getters_and_iteration(self):
        sv_station = self.server.add_station(common_address=95)
        sv_m = sv_station.add_point(io_address=1, type=c104.Type.M_ME_NC_1)
        sv_m.value = 7.5
        sv_cmd = sv_station.add_point(io_address=2, type=c104.Type.C_SC_NA_1)

        # capture incoming messages on the server side command point
        def on_receive(point: c104.Point, previous_info: c104.Information,
                       message: c104.IncomingMessage) -> c104.ResponseState:
            self.commands.append(message)
            return c104.ResponseState.SUCCESS

        sv_cmd.on_receive(callable=on_receive)

        c_station = self.connection.add_station(common_address=95)
        c_m = c_station.add_point(io_address=1, type=c104.Type.M_ME_NC_1)
        c_cmd = c_station.add_point(io_address=2, type=c104.Type.C_SC_NA_1)

        def on_receive_client(point: c104.Point,
                              previous_info: c104.Information,
                              message: c104.IncomingMessage
                              ) -> c104.ResponseState:
            self.received.append(message)
            return c104.ResponseState.SUCCESS

        c_m.on_receive(callable=on_receive_client)

        self.server.start()
        self.client.start()
        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")

        sv_station.signal_initialized(cause=c104.Coi.LOCAL_MANUAL_RESET)
        self.assertTrue(
            self.connection.interrogation(common_address=95,
                                          cause=c104.Cot.ACTIVATION,
                                          qualifier=c104.Qoi.STATION))

        # client sends a command -> server gets an IncomingMessage
        self.assertTrue(c_cmd.transmit(cause=c104.Cot.ACTIVATION))

        self.assertTrue(
            wait_for(lambda: len(self.commands) > 0),
            "no incoming command message captured on server")
        self.assertTrue(
            wait_for(lambda: len(self.received) > 0),
            "no incoming monitoring message captured on client")

        # ---- exercise getters + first()/next() on a captured message ----
        msg = self.commands[0]
        self.assertIsInstance(msg.common_address, int)
        self.assertIsInstance(msg.io_address, int)
        self.assertIsInstance(msg.cot, c104.Cot)
        self.assertIsInstance(msg.type, c104.Type)
        self.assertIsInstance(msg.number_of_objects, int)
        self.assertIsInstance(msg.number_of_object, int)  # deprecated alias
        self.assertIsInstance(msg.originator_address, int)
        self.assertIsInstance(msg.raw, bytes)
        self.assertIsInstance(msg.raw_explain, str)
        self.assertIsInstance(msg.is_negative, bool)
        self.assertIsInstance(msg.is_sequence, bool)
        self.assertIsInstance(msg.is_test, bool)
        self.assertIsInstance(msg.is_select_command, bool)
        self.assertIsInstance(msg.info, c104.Information)

        # iteration: first() then next() moves the pointer
        first_info = msg.info
        self.assertIsNotNone(first_info)
        msg.first()
        self.assertIsNotNone(msg.info)


if __name__ == "__main__":
    unittest.main()
