#!/usr/bin/env python3
"""
unittest suite covering every Information / command-info class exposed by the
c104 type stub (c104/__init__.pyi).

For each class we:
  * construct it with valid arguments (ctor signatures follow the bindings)
  * read every listed property
  * call repr()/str() on the info object
Then, for each information category, we demonstrate how a matching c104.Type
point yields that info object via point.info and a point.value round-trip.

IMPORTANT: this suite must be run with the INSTALLED c104 package (NOT the
in-tree c104/ source dir). Run it from a directory other than the repo root:
    cd /tmp && python3 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_info.py'
    # or with coverage:
    cd /tmp && coverage run --source c104 -m unittest discover -s \
        /path/to/iec104-python/tests -p 'test_pyi_info.py' && coverage report
"""

import datetime
import unittest

import c104


def make_server(port=24301):
    return c104.Server(ip="127.0.0.1", port=port, tick_rate_ms=100)


def as_int(obj):
    return int(obj)


def as_bytes(obj):
    return bytes(obj)


class InformationBaseTest(unittest.TestCase):
    """Shared assertions for the Information base interface."""

    def _check_base(self, info):
        self.assertIsInstance(info.is_readonly, bool)
        self.assertIsInstance(info.processed_at, datetime.datetime)
        self.assertTrue(info.recorded_at is None or
                        isinstance(info.recorded_at, datetime.datetime))
        self.assertIsNotNone(repr(info))
        self.assertIsNotNone(str(info))


class SingleInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.SingleInfo(True, c104.Quality.Invalid,
                               datetime.datetime.now())
        self.assertTrue(info.on)
        self.assertTrue(info.value)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self._check_base(info)

    def test_defaults(self):
        info = c104.SingleInfo(False)
        self.assertFalse(info.on)
        self.assertFalse(info.value)
        self.assertIsNotNone(info.quality)

    def test_quality_setter_via_point(self):
        server = make_server(24311)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
        p.quality = c104.Quality.Invalid
        self.assertEqual(p.quality, c104.Quality.Invalid)
        server.stop()

    def test_point_roundtrip(self):
        server = make_server()
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=1, type=c104.Type.M_SP_NA_1)
        p.value = True
        self.assertIsInstance(p.info, c104.SingleInfo)
        self.assertEqual(p.value, True)
        p.info = c104.SingleInfo(False)
        self.assertFalse(p.info.value)
        server.stop()

    def test_point_roundtrip_tb(self):
        server = make_server(24312)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=2, type=c104.Type.M_SP_TB_1)
        p.value = True
        self.assertIsInstance(p.info, c104.SingleInfo)
        self.assertEqual(p.value, True)
        server.stop()


class DoubleInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.DoubleInfo(c104.Double.ON, c104.Quality.Overflow)
        self.assertEqual(info.state, c104.Double.ON)
        self.assertEqual(info.value, c104.Double.ON)
        self.assertEqual(info.quality, c104.Quality.Overflow)
        self._check_base(info)

    def test_defaults(self):
        info = c104.DoubleInfo(c104.Double.OFF)
        self.assertEqual(info.state, c104.Double.OFF)
        self.assertEqual(info.value, c104.Double.OFF)

    def test_point_roundtrip(self):
        server = make_server(24313)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=10, type=c104.Type.M_DP_NA_1)
        p.value = c104.Double.ON
        self.assertIsInstance(p.info, c104.DoubleInfo)
        self.assertEqual(p.value, c104.Double.ON)
        p.value = c104.Double.OFF
        self.assertEqual(p.value, c104.Double.OFF)
        server.stop()


class StepInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.StepInfo(c104.Int7(2), True, c104.Quality.Invalid)
        self.assertEqual(as_int(info.position), 2)
        self.assertEqual(as_int(info.value), 2)
        self.assertTrue(info.transient)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self._check_base(info)

    def test_transient_prop(self):
        info = c104.StepInfo(c104.Int7(-3), False)
        self.assertFalse(info.transient)
        self.assertEqual(as_int(info.position), -3)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24314)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=20, type=c104.Type.M_ST_NA_1)
        p.value = c104.Int7(42)
        self.assertIsInstance(p.info, c104.StepInfo)
        self.assertEqual(as_int(p.value), 42)
        server.stop()


class ScaledInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.ScaledInfo(c104.Int16(-2345), c104.Quality.Substituted)
        self.assertEqual(as_int(info.actual), -2345)
        self.assertEqual(as_int(info.value), -2345)
        self.assertEqual(info.quality, c104.Quality.Substituted)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24315)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=30, type=c104.Type.M_ME_NB_1)
        p.value = c104.Int16(-123)
        self.assertIsInstance(p.info, c104.ScaledInfo)
        self.assertEqual(as_int(p.value), -123)
        server.stop()


class ShortInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.ShortInfo(23.45, c104.Quality.Blocked)
        self.assertAlmostEqual(info.actual, 23.45, places=4)
        self.assertAlmostEqual(info.value, 23.45, places=4)
        self.assertEqual(info.quality, c104.Quality.Blocked)
        self._check_base(info)

    def test_defaults(self):
        info = c104.ShortInfo(0.0)
        self.assertEqual(info.value, 0.0)
        self.assertIsNotNone(info.quality)

    def test_point_roundtrip(self):
        server = make_server(24316)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=40, type=c104.Type.M_ME_NC_1)
        p.value = -1234.5
        self.assertIsInstance(p.info, c104.ShortInfo)
        self.assertAlmostEqual(p.value, -1234.5, places=3)
        server.stop()


class NormalizedInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.NormalizedInfo(c104.NormalizedFloat(0.5),
                                   c104.Quality.Invalid)
        self.assertAlmostEqual(float(info.actual), 0.5, places=5)
        self.assertAlmostEqual(float(info.value), 0.5, places=5)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self._check_base(info)

    def test_point_roundtrip_na(self):
        server = make_server(24317)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=50, type=c104.Type.M_ME_NA_1)
        p.value = c104.NormalizedFloat(0.25)
        self.assertIsInstance(p.info, c104.NormalizedInfo)
        self.assertAlmostEqual(float(p.value), 0.25, places=5)
        server.stop()

    def test_point_roundtrip_nd(self):
        # M_ME_ND_1 maps to NormalizedInfo (values are NormalizedFloat)
        server = make_server(24318)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=51, type=c104.Type.M_ME_ND_1)
        p.value = c104.NormalizedFloat(0.75)
        self.assertIsInstance(p.info, c104.NormalizedInfo)
        self.assertAlmostEqual(float(p.value), 0.75, places=5)
        server.stop()


class BinaryInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        blob = c104.Byte32(0b101010)
        info = c104.BinaryInfo(blob, c104.Quality.NonTopical)
        self.assertEqual(as_bytes(info.blob), as_bytes(blob))
        self.assertEqual(as_bytes(info.value), as_bytes(blob))
        self.assertEqual(info.quality, c104.Quality.NonTopical)
        self._check_base(info)

    def test_byte32_helpers(self):
        b = c104.Byte32(1234)
        # bytes() is little-endian (least significant byte first), matching the
        # IEC 60870-5-4 wire format used by lib60870.
        self.assertEqual(as_bytes(b), (1234).to_bytes(4, "little"))
        self.assertIsNotNone(repr(b))
        self.assertIsNotNone(str(b))

    def test_point_roundtrip(self):
        server = make_server(24319)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=60, type=c104.Type.M_BO_NA_1)
        p.value = c104.Byte32(0b101)
        self.assertIsInstance(p.info, c104.BinaryInfo)
        self.assertEqual(as_bytes(p.value), as_bytes(c104.Byte32(0b101)))
        server.stop()


class BinaryCounterInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.BinaryCounterInfo(2345, c104.UInt5(7),
                                      c104.BinaryCounterQuality.Invalid)
        self.assertEqual(info.counter, 2345)
        self.assertEqual(info.value, 2345)
        self.assertEqual(as_int(info.sequence), 7)
        self.assertEqual(info.quality, c104.BinaryCounterQuality.Invalid)
        self.assertIsInstance(info.quality.value, int)
        self.assertIsInstance(info.quality.is_any(), bool)
        self.assertIsInstance(info.quality.is_good(), bool)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24320)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=70, type=c104.Type.M_IT_NA_1)
        p.value = 999
        self.assertIsInstance(p.info, c104.BinaryCounterInfo)
        self.assertEqual(p.value, 999)
        server.stop()


class ProtectionCircuitInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        circuits = c104.OutputCircuits.PhaseL1 | c104.OutputCircuits.PhaseL2
        info = c104.ProtectionCircuitInfo(circuits, c104.UInt16(35000),
                                          c104.Quality.Invalid)
        self.assertEqual(info.circuits, circuits)
        self.assertEqual(info.value, circuits)
        self.assertEqual(as_int(info.relay_operating_ms), 35000)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self.assertIsInstance(info.circuits.is_any(), bool)
        self.assertIsInstance(info.circuits.is_none(), bool)
        self.assertIsNotNone(str(info.circuits))
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24321)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=80, type=c104.Type.M_EP_TF_1)
        p.value = c104.OutputCircuits.General
        self.assertIsInstance(p.info, c104.ProtectionCircuitInfo)
        self.assertEqual(p.value, c104.OutputCircuits.General)
        server.stop()


class ProtectionEventInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.ProtectionEventInfo(c104.EventState.ON, c104.UInt16(1500),
                                        c104.Quality.Invalid)
        self.assertEqual(info.state, c104.EventState.ON)
        self.assertEqual(info.value, c104.EventState.ON)
        self.assertEqual(as_int(info.elapsed_ms), 1500)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24322)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=90, type=c104.Type.M_EP_TD_1)
        p.value = c104.EventState.ON
        self.assertIsInstance(p.info, c104.ProtectionEventInfo)
        self.assertEqual(p.value, c104.EventState.ON)
        server.stop()


class ProtectionStartInfoTest(InformationBaseTest):
    def test_ctor_and_props(self):
        events = c104.StartEvents.PhaseL1 | c104.StartEvents.PhaseL3
        info = c104.ProtectionStartInfo(events, c104.UInt16(3000),
                                        c104.Quality.Invalid)
        self.assertEqual(info.events, events)
        self.assertEqual(info.value, events)
        self.assertEqual(as_int(info.relay_duration_ms), 3000)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self.assertIsInstance(info.events.is_any(), bool)
        self.assertIsInstance(info.events.is_none(), bool)
        self.assertIsNotNone(str(info.events))
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24323)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=91, type=c104.Type.M_EP_TE_1)
        p.value = c104.StartEvents.General
        self.assertIsInstance(p.info, c104.ProtectionStartInfo)
        self.assertEqual(p.value, c104.StartEvents.General)
        server.stop()


class StatusAndChangedTest(InformationBaseTest):
    def test_ctor_and_props(self):
        status = c104.PackedSingle.I0 | c104.PackedSingle.I5
        changed = c104.PackedSingle.I3
        info = c104.StatusAndChanged(status, changed, c104.Quality.Invalid)
        self.assertEqual(info.status, status)
        self.assertEqual(info.value, status)
        self.assertEqual(info.changed, changed)
        self.assertEqual(info.quality, c104.Quality.Invalid)
        self.assertIsInstance(info.status.is_any(), bool)
        self.assertIsInstance(info.status.is_none(), bool)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24324)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=92, type=c104.Type.M_PS_NA_1)
        p.value = c104.PackedSingle.I0
        self.assertIsInstance(p.info, c104.StatusAndChanged)
        self.assertEqual(p.value, c104.PackedSingle.I0)
        server.stop()


# ---------------------------------------------------------------------------
# Command info classes (control points)
# ---------------------------------------------------------------------------

class SingleCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.SingleCmd(True, c104.Qoc.SHORT_PULSE)
        self.assertTrue(info.on)
        self.assertTrue(info.value)
        self.assertEqual(info.qualifier, c104.Qoc.SHORT_PULSE)
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24325)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=100, type=c104.Type.C_SC_NA_1)
        p.info = c104.SingleCmd(True)
        self.assertIsInstance(p.info, c104.SingleCmd)
        self.assertTrue(p.info.value)
        server.stop()


class DoubleCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.DoubleCmd(c104.Double.ON, c104.Qoc.LONG_PULSE)
        self.assertEqual(info.state, c104.Double.ON)
        self.assertEqual(info.value, c104.Double.ON)
        self.assertEqual(info.qualifier, c104.Qoc.LONG_PULSE)
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24326)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=101, type=c104.Type.C_DC_NA_1)
        p.info = c104.DoubleCmd(c104.Double.OFF)
        self.assertIsInstance(p.info, c104.DoubleCmd)
        self.assertEqual(p.info.value, c104.Double.OFF)
        server.stop()


class StepCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.StepCmd(c104.Step.HIGHER, c104.Qoc.SHORT_PULSE)
        self.assertEqual(info.direction, c104.Step.HIGHER)
        self.assertEqual(info.value, c104.Step.HIGHER)
        self.assertEqual(info.qualifier, c104.Qoc.SHORT_PULSE)
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24327)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=102, type=c104.Type.C_RC_NA_1)
        p.info = c104.StepCmd(c104.Step.LOWER)
        self.assertIsInstance(p.info, c104.StepCmd)
        self.assertEqual(p.info.value, c104.Step.LOWER)
        server.stop()


class ScaledCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.ScaledCmd(c104.Int16(-2345), c104.UInt7(7))
        self.assertEqual(as_int(info.target), -2345)
        self.assertEqual(as_int(info.value), -2345)
        self.assertEqual(as_int(info.qualifier), 7)
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24328)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=103, type=c104.Type.C_SE_NB_1)
        p.info = c104.ScaledCmd(c104.Int16(100))
        self.assertIsInstance(p.info, c104.ScaledCmd)
        self.assertEqual(as_int(p.info.value), 100)
        server.stop()


class ShortCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.ShortCmd(-23.45, c104.UInt7(3))
        self.assertAlmostEqual(info.target, -23.45, places=4)
        self.assertAlmostEqual(info.value, -23.45, places=4)
        self.assertEqual(as_int(info.qualifier), 3)
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24329)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=104, type=c104.Type.C_SE_NC_1)
        p.info = c104.ShortCmd(13.45)
        self.assertIsInstance(p.info, c104.ShortCmd)
        self.assertAlmostEqual(p.info.value, 13.45, places=3)
        server.stop()


class NormalizedCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        info = c104.NormalizedCmd(c104.NormalizedFloat(-0.5), c104.UInt7(5))
        self.assertAlmostEqual(float(info.target), -0.5, places=5)
        self.assertAlmostEqual(float(info.value), -0.5, places=5)
        self.assertEqual(as_int(info.qualifier), 5)
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24330)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=105, type=c104.Type.C_SE_NA_1)
        p.info = c104.NormalizedCmd(c104.NormalizedFloat(0.5))
        self.assertIsInstance(p.info, c104.NormalizedCmd)
        self.assertAlmostEqual(float(p.info.value), 0.5, places=5)
        server.stop()


class BinaryCmdTest(InformationBaseTest):
    def test_ctor_and_props(self):
        blob = c104.Byte32(0b110011)
        info = c104.BinaryCmd(blob)
        self.assertEqual(as_bytes(info.blob), as_bytes(blob))
        self.assertEqual(as_bytes(info.value), as_bytes(blob))
        self.assertIsNone(info.quality)
        self._check_base(info)

    def test_point_roundtrip(self):
        server = make_server(24331)
        station = server.add_station(common_address=1)
        p = station.add_point(io_address=106, type=c104.Type.C_BO_NA_1)
        p.info = c104.BinaryCmd(c104.Byte32(0b1))
        self.assertIsInstance(p.info, c104.BinaryCmd)
        self.assertEqual(as_bytes(p.info.value), as_bytes(c104.Byte32(0b1)))
        server.stop()


if __name__ == "__main__":
    unittest.main()
