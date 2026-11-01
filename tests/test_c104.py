#!/usr/bin/env python3
"""
Copyright 2026 Fraunhofer Institute for Applied Information Technology FIT

This file is part of iec104-python.
... (GPL header)

Python unittest suite for the c104 package (installed via `pip install $PWD`).

IMPORTANT: this suite must be run with the installed c104 package (NOT the
in-tree c104/ source directory). Run it from a directory other than the repo
root, e.g.:
    cd /tmp && python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_c104.py'
    # or with coverage:
    cd /tmp && coverage run --source c104 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_c104.py' && coverage report
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


class ServerClientLoopbackTestCase(unittest.TestCase):
    """Real server<->client round trips over loopback on a distinct port."""

    PORT = 24101

    def setUp(self):
        self.server = c104.Server(ip="127.0.0.1", port=self.PORT,
                                  tick_rate_ms=100)
        self.client = c104.Client(tick_rate_ms=100, command_timeout_ms=3000)
        self.connection = self.client.add_connection(
            ip="127.0.0.1", port=self.PORT,
            init=c104.Init.INTERROGATION)

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

    def test_server_client_connect_and_interrogate(self):
        sv_station = self.server.add_station(common_address=47)
        m_point = sv_station.add_point(io_address=11, type=c104.Type.M_ME_NC_1,
                                       report_ms=0)
        m_point.value = 12.34

        c_station = self.connection.add_station(common_address=47)
        c_station.add_point(io_address=11, type=c104.Type.M_ME_NC_1)

        self.server.start()
        self.client.start()

        # end of initialization so client sees the station/point
        sv_station.signal_initialized(cause=c104.Coi.LOCAL_MANUAL_RESET)

        self.assertTrue(
            wait_for(lambda: self.connection.is_connected),
            "connection never became connected")

    def test_point_value_quality_roundtrip(self):
        sv_station = self.server.add_station(common_address=10)
        p = sv_station.add_point(io_address=5, type=c104.Type.M_SP_NA_1)
        p.value = True
        self.assertEqual(p.value, True)
        self.assertIsNotNone(p.io_address)
        self.assertEqual(p.io_address, 5)
        self.assertIsNotNone(p.type)
        # quality default
        self.assertIsNotNone(p.info)
        self.assertIsNotNone(str(p))
        self.assertIsNotNone(str(sv_station))

    def test_client_creation(self):
        self.assertIsNotNone(self.client)
        self.assertIsNotNone(self.connection)
        self.assertEqual(self.connection.ip, "127.0.0.1")
        self.assertEqual(self.connection.port, self.PORT)


class VariantValueTestCase(unittest.TestCase):
    def test_value_classes_exist(self):
        for cls in (c104.Byte32,):
            self.assertTrue(hasattr(cls, "__call__"))

    def test_float_point_variant(self):
        server = c104.Server(ip="127.0.0.1", port=24102, tick_rate_ms=100)
        station = server.add_station(common_address=47)
        p = station.add_point(io_address=1, type=c104.Type.M_ME_NC_1)
        p.value = -1234.5
        self.assertAlmostEqual(p.value, -1234.5, places=3)
        server.stop()

    def test_double_point_variant(self):
        server = c104.Server(ip="127.0.0.1", port=24103, tick_rate_ms=100)
        station = server.add_station(common_address=40)
        p = station.add_point(io_address=2, type=c104.Type.M_DP_TB_1)
        p.value = c104.Double.ON
        self.assertEqual(p.value, c104.Double.ON)
        p.value = c104.Double.OFF
        self.assertEqual(p.value, c104.Double.OFF)
        server.stop()

    def test_step_point_variant(self):
        server = c104.Server(ip="127.0.0.1", port=24104, tick_rate_ms=100)
        station = server.add_station(common_address=40)
        p = station.add_point(io_address=3, type=c104.Type.M_ST_TB_1)
        p.value = c104.Int7(42)
        self.assertIsNotNone(p.value)
        server.stop()

    def test_debug_mode_functions(self):
        prev = c104.get_debug_mode()
        c104.set_debug_mode(c104.Debug.Server | c104.Debug.Client)
        self.assertIsNotNone(c104.get_debug_mode())
        c104.set_debug_mode(prev)

    def test_quality_getter(self):
        server = c104.Server(ip="127.0.0.1", port=24105, tick_rate_ms=100)
        station = server.add_station(common_address=41)
        p = station.add_point(io_address=4, type=c104.Type.M_ME_NC_1)
        self.assertIsNotNone(p.quality)
        server.stop()


class PointCallbackTestCase(unittest.TestCase):
    PORT = 24110

    def test_callback_attached(self):
        server = c104.Server(ip="127.0.0.1", port=self.PORT, tick_rate_ms=100)
        station = server.add_station(common_address=50)
        fired = []

        def on_before(point: c104.Point) -> None:
            fired.append(point.io_address)

        p = station.add_point(io_address=9, type=c104.Type.M_ME_NC_1,
                              report_ms=100)
        p.on_before_auto_transmit(callable=on_before)
        # attaching the strict-signature callback must not raise
        self.assertEqual(p.report_ms, 100)
        self.assertEqual(len(fired), 0)


if __name__ == "__main__":
    unittest.main()
