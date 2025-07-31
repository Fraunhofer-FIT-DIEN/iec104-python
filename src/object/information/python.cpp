/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied Information Technology
 * FIT
 *
 * This file is part of iec104-python.
 * iec104-python is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * iec104-python is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with iec104-python. If not, see <https://www.gnu.org/licenses/>.
 *
 *  See LICENSE file for the complete license text.
 *
 *
 * @file InformationPy.cpp
 * @brief python binding for Information class and derived classes
 *
 * @package iec104-python
 * @namespace Object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "object/information/BinaryCmd.h"
#include "object/information/BinaryInfo.h"
#include "object/information/DoubleCmd.h"
#include "object/information/DoubleInfo.h"
#include "object/information/EquipmentProtectionEvent.h"
#include "object/information/EquipmentProtectionOutputCircuitEvent.h"
#include "object/information/EquipmentProtectionStartEvents.h"
#include "object/information/IInformation.h"
#include "object/information/IntegratedTotalInfo.h"
#include "object/information/NormalizedCmd.h"
#include "object/information/NormalizedInfo.h"
#include "object/information/ScaledCmd.h"
#include "object/information/ScaledInfo.h"
#include "object/information/ShortCmd.h"
#include "object/information/ShortInfo.h"
#include "object/information/SingleCmd.h"
#include "object/information/SingleInfo.h"
#include "object/information/StatusWithChangeDetection.h"
#include "object/information/StepCmd.h"
#include "object/information/StepInfo.h"
#include "transformer/Type.h"

using namespace pybind11::literals;
using namespace Object::Information;

void init_object_information(py::module_ &m) {

  py::class_<IInformation, std::shared_ptr<IInformation>>(
      m, "Information",
      "This class represents all specialized kind of information a specific "
      "point may have")
      .def_static("from_type", &Transformer::fromType,
                  R"def(from_type(type: c104.Type) -> c104.Information

create an empty information object from a IEC message type

Parameters
----------
type: c104.Type
    point information type

Returns
-------
c104.Information
    new information object

Raises
------
ValueError
    if type not supported
)def",
                  "callable"_a)
      .def_property_readonly(
          "value", &IInformation::getValue,
          R"def(typing.Union[None, bool, c104.Double, c104.Step, c104.Int7, c104.Int16, int, c104.Byte32, c104.NormalizedFloat, float, c104.EventState, c104.StartEvents, c104.OutputCircuits, c104.PackedSingle]: the mapped primary information value property (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &IInformation::getQuality,
          R"def(typing.Union[None, c104.Quality, c104.BinaryCounterQuality]: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def_property_readonly(
          "processed_at", &IInformation::getProcessedAt,
          "c104.DateTime: timestamp with milliseconds of last local "
          "information processing "
          "(read-only)")
      .def_property_readonly("recorded_at", &IInformation::getRecordedAt,
                             "c104.DateTime | None : timestamp with "
                             "milliseconds transported with the "
                             "value "
                             "itself or None (read-only)")
      .def_property_readonly("is_readonly", &IInformation::isReadonly,
                             "bool: test if the information is read-only")
      .def(
          "as_type",
          [](const std::shared_ptr<IInformation> &o, const bool timestamp) {
            return Transformer::asType(o, timestamp);
          },
          R"def(as_type(self: c104.Information, timestamp: bool) -> c104.Type

get related IEC60870 message type identifier (with or without timestamp)

Parameters
----------
timestamp: bool
    identifier with or without timestamp

Returns
-------
c104.Type

Raises
------
ValueError
    if the information type is not supported
)def",
          "timestamp"_a)
      .def("__repr__", &IInformation::toString);

  py::class_<SingleInfo, IInformation, std::shared_ptr<SingleInfo>>(
      m, "SingleInfo",
      "This class represents all specific single point information")
      .def(
          py::init(&SingleInfo::create),
          R"def(__init__(self: c104.SingleInfo, on: bool, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new single info

Parameters
----------
on: bool
    Single status value
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> single_info = c104.SingleInfo(on=True, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "on"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("on", &SingleInfo::isOn,
                             "bool: the value (read-only)")
      .def_property_readonly("value", &SingleInfo::getValue,
                             R"def(bool: references property ``on`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &SingleInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &SingleInfo::toString);

  py::class_<SingleCmd, IInformation, std::shared_ptr<SingleCmd>>(
      m, "SingleCmd",
      "This class represents all specific single command information")
      .def(
          py::init(&SingleCmd::create),
          R"def(__init__(self: c104.SingleCmd, on: bool, qualifier: c104.Qoc = c104.Qoc.NONE, recorded_at: c104.DateTime = None) -> None

create a new single command

Parameters
----------
on: bool
    Single command value
qualifier: c104.Qoc
    Qualifier of command
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> single_cmd = c104.SingleCmd(on=True, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "on"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
          "recorded_at"_a = py::none())
      .def_property_readonly("on", &SingleCmd::isOn,
                             "bool: the value (read-only)")
      .def_property_readonly("value", &SingleCmd::getValue,
                             R"def(bool: references property ``on`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &SingleCmd::getQuality,
          "None: This information does not contain quality information.")
      .def_property_readonly(
          "qualifier", &SingleCmd::getQualifier,
          "c104.Qoc: the command qualifier information (read-only)")
      .def("__repr__", &SingleCmd::toString);

  py::class_<DoubleInfo, IInformation, std::shared_ptr<DoubleInfo>>(
      m, "DoubleInfo",
      "This class represents all specific double point information")
      .def(
          py::init(&DoubleInfo::create),
          R"def(__init__(self: c104.DoubleInfo, state: c104.Double | int, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new double info

Parameters
----------
state: c104.Double | int
    Double point status value
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> double_info = c104.DoubleInfo(state=c104.Double.ON, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("state", &DoubleInfo::getState,
                             "c104.Double: the value (read-only)")
      .def_property_readonly(
          "value", &DoubleInfo::getValue,
          R"def(c104.Double: references property ``state`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &DoubleInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &DoubleInfo::toString);

  py::class_<DoubleCmd, IInformation, std::shared_ptr<DoubleCmd>>(
      m, "DoubleCmd",
      "This class represents all specific double command information")
      .def(
          py::init(&DoubleCmd::create),
          R"def(__init__(self: c104.DoubleCmd, state: c104.Double | int, qualifier: c104.Qoc = c104.Qoc.NONE, recorded_at: c104.DateTime = None) -> None

create a new double command

Parameters
----------
state: c104.Double | int
    Double command value
qualifier: c104.Qoc
    Qualifier of command
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> double_cmd = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
          "recorded_at"_a = py::none())
      .def_property_readonly("state", &DoubleCmd::getState,
                             "c104.Double: the value (read-only)")
      .def_property_readonly(
          "qualifier", &DoubleCmd::getQualifier,
          "c104.Qoc: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &DoubleCmd::getValue,
          R"def(c104.Double: references property ``state`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &DoubleCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &DoubleCmd::toString);

  py::class_<StepInfo, IInformation, std::shared_ptr<StepInfo>>(
      m, "StepInfo",
      "This class represents all specific step point information")
      .def(
          py::init(&StepInfo::create),
          R"def(__init__(self: c104.StepInfo, position: c104.Int7 | int, transient: bool, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new step info

Parameters
----------
position: c104.Int7 | int
    Current transformer step position value
transient: bool
    Indicator, if transformer is currently in step change procedure
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> step_info = c104.StepInfo(position=c104.Int7(2), transient=False, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "transient"_a = false, "quality"_a = Quality::None,
          "recorded_at"_a = py::none())
      .def_property_readonly("position", &StepInfo::getPosition,
                             "c104.Int7: the value (read-only)")
      .def_property_readonly("transient", &StepInfo::isTransient,
                             "bool: if the position is transient (read-only)")
      .def_property_readonly(
          "value", &StepInfo::getValue,
          R"def(c104.Int7: references property ``position`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &StepInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &StepInfo::toString);

  py::class_<StepCmd, IInformation, std::shared_ptr<StepCmd>>(
      m, "StepCmd",
      "This class represents all specific step command information")
      .def(
          py::init(&StepCmd::create),
          R"def(__init__(self: c104.StepCmd, direction: c104.Step | int, qualifier: c104.Qoc = c104.Qoc.NONE, recorded_at: c104.DateTime = None) -> None

create a new step command

Parameters
----------
direction: c104.Step | int
    Step command direction value
qualifier: c104.Qoc
    Qualifier of Command
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> step_cmd = c104.StepCmd(direction=c104.Step.HIGHER, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "direction"_a, "qualifier"_a = CS101_QualifierOfCommand::NONE,
          "recorded_at"_a = py::none())
      .def_property_readonly("direction", &StepCmd::getStep,
                             "c104.Step: the value (read-only)")
      .def_property_readonly(
          "qualifier", &StepCmd::getQualifier,
          "c104.Qoc: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &DoubleCmd::getValue,
          R"def(c104.Step: references property ``direction`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &DoubleCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &StepCmd::toString);

  py::class_<BinaryInfo, IInformation, std::shared_ptr<BinaryInfo>>(
      m, "BinaryInfo",
      "This class represents all specific binary point information")
      .def(
          py::init(&BinaryInfo::create),
          R"def(__init__(self: c104.BinaryInfo, blob: c104.Byte32, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new binary info

Parameters
----------
blob: c104.Byte32
    Binary status value
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> binary_info = c104.BinaryInfo(blob=c104.Byte32(2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "blob"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("blob", &BinaryInfo::getBlob,
                             "c104.Byte32: the value (read-only)")
      .def_property_readonly(
          "value", &BinaryInfo::getValue,
          R"def(c104.Byte32: references property ``blob`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &BinaryInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &BinaryInfo::toString);

  py::class_<BinaryCmd, IInformation, std::shared_ptr<BinaryCmd>>(
      m, "BinaryCmd",
      "This class represents all specific binary command information")
      .def(
          py::init(&BinaryCmd::create),
          R"def(__init__(self: c104.BinaryCmd, blob: c104.Byte32, recorded_at: c104.DateTime = None) -> None

create a new binary command

Parameters
----------
blob: c104.Byte32
    Binary command value
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> binary_cmd = c104.BinaryCmd(blob=c104.Byte32(1234), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "blob"_a, "recorded_at"_a = py::none())
      .def_property_readonly("blob", &BinaryCmd::getBlob,
                             "c104.Byte32: the value (read-only)")
      .def_property_readonly(
          "value", &BinaryCmd::getValue,
          R"def(c104.Byte32: references property ``blob`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &BinaryCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &BinaryCmd::toString);

  py::class_<NormalizedInfo, IInformation, std::shared_ptr<NormalizedInfo>>(
      m, "NormalizedInfo",
      "This class represents all specific normalized measurement point "
      "information")
      .def(
          py::init(&NormalizedInfo::create),
          R"def(__init__(self: c104.NormalizedInfo, actual: c104.NormalizedFloat, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new normalized measurement info

Parameters
----------
actual: c104.NormalizedFloat
    Actual measurement value [-1.f, 1.f]
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> normalized_info = c104.NormalizedInfo(actual=c104.NormalizedFloat(23.45), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "actual"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("actual", &NormalizedInfo::getActual,
                             "c104.NormalizedFloat: the value (read-only)")
      .def_property_readonly(
          "value", &NormalizedInfo::getValue,
          R"def(c104.NormalizedFloat: references property ``actual`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &NormalizedInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &NormalizedInfo::toString);

  py::class_<NormalizedCmd, IInformation, std::shared_ptr<NormalizedCmd>>(
      m, "NormalizedCmd",
      "This class represents all specific normalized set point command "
      "information")
      .def(
          py::init(&NormalizedCmd::create),
          R"def(__init__(self: c104.NormalizedCmd, target: c104.NormalizedFloat, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: c104.DateTime = None) -> None

create a new normalized set point command

Parameters
----------
target: c104.NormalizedFloat
    Target set-point value [-1.f, 1.f]
qualifier: c104.UInt7
    Qualifier of set-point command
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> normalized_cmd = c104.NormalizedCmd(target=c104.NormalizedFloat(23.45), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "target"_a, "qualifier"_a = LimitedUInt7(0),
          "recorded_at"_a = py::none())
      .def_property_readonly("target", &NormalizedCmd::getTarget,
                             "c104.NormalizedFloat: the value (read-only)")
      .def_property_readonly(
          "qualifier", &NormalizedCmd::getQualifier,
          "c104.UInt7: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &NormalizedCmd::getValue,
          R"def(c104.NormalizedFloat: references property ``target`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &NormalizedCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &NormalizedCmd::toString);

  py::class_<ScaledInfo, IInformation, std::shared_ptr<ScaledInfo>>(
      m, "ScaledInfo",
      "This class represents all specific scaled measurement point information")
      .def(
          py::init(&ScaledInfo::create),
          R"def(__init__(self: c104.ScaledInfo, actual: c104.Int16 | int, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new scaled measurement info

Parameters
----------
actual: c104.Int16 | int
    Actual measurement value [-32768, 32767]
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> scaled_info = c104.ScaledInfo(actual=c104.Int16(-2345), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "actual"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("actual", &ScaledInfo::getActual,
                             "c104.Int16: the value (read-only)")
      .def_property_readonly(
          "value", &ScaledInfo::getValue,
          R"def(c104.Int16: references property ``actual`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &ScaledInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &ScaledInfo::toString);

  py::class_<ScaledCmd, IInformation, std::shared_ptr<ScaledCmd>>(
      m, "ScaledCmd",
      "This class represents all specific scaled set point command information")
      .def(
          py::init(&ScaledCmd::create),
          R"def(__init__(self: c104.ScaledCmd, target: c104.Int16, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: c104.DateTime = None) -> None

create a new scaled set point command

Parameters
----------
target: c104.Int16
    Target set-point value [-32768, 32767]
qualifier: c104.UInt7
    Qualifier of set-point command
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> scaled_cmd = c104.ScaledCmd(target=c104.Int16(-2345), qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "target"_a, "qualifier"_a = LimitedUInt7(0),
          "recorded_at"_a = py::none())
      .def_property_readonly("target", &ScaledCmd::getTarget,
                             "c104.Int16: the value (read-only)")
      .def_property_readonly(
          "qualifier", &ScaledCmd::getQualifier,
          "c104.UInt7: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &ScaledCmd::getValue,
          R"def(c104.Int16: references property ``target`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &ScaledCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &ScaledCmd::toString);

  py::class_<ShortInfo, IInformation, std::shared_ptr<ShortInfo>>(
      m, "ShortInfo",
      "This class represents all specific short measurement point information")
      .def(
          py::init(&ShortInfo::create),
          R"def(__init__(self: c104.ShortInfo, actual: float, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new short measurement info

Parameters
----------
actual: float
    Actual measurement value in 32-bit precision
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> short_info = c104.ShortInfo(actual=23.45, quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "actual"_a, "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("actual", &ShortInfo::getActual,
                             "float: the value (read-only)")
      .def_property_readonly(
          "value", &ShortInfo::getValue,
          R"def(float: references property ``actual`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &ShortInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &ShortInfo::toString);

  py::class_<ShortCmd, IInformation, std::shared_ptr<ShortCmd>>(
      m, "ShortCmd",
      "This class represents all specific short set point command information")
      .def(
          py::init(&ShortCmd::create),
          R"def(__init__(self: c104.ShortCmd, target: float, qualifier: c104.UInt7 = c104.UInt7(0), recorded_at: c104.DateTime = None) -> None

create a new short set point command

Parameters
----------
target: float
    Target set-point value in 32-bit precision
qualifier: c104.UInt7
    Qualifier of set-point command
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> short_cmd = c104.ShortCmd(target=-23.45, qualifier=c104.UInt7(123), recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "target"_a, "qualifier"_a = LimitedUInt7(0),
          "recorded_at"_a = py::none())
      .def_property_readonly("target", &ShortCmd::getTarget,
                             "float: the value (read-only)")
      .def_property_readonly(
          "qualifier", &ShortCmd::getQualifier,
          "c104.UInt7: the command qualifier information (read-only)")
      .def_property_readonly(
          "value", &ShortCmd::getValue,
          R"def(float: references property ``target`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &ShortCmd::getQuality,
          "None: This information does not contain quality information.")
      .def("__repr__", &ShortCmd::toString);

  py::class_<BinaryCounterInfo, IInformation,
             std::shared_ptr<BinaryCounterInfo>>(
      m, "BinaryCounterInfo",
      "This class represents all specific integrated totals of binary counter "
      "point information")
      .def(
          py::init(&BinaryCounterInfo::create),
          R"def(__init__(self: c104.BinaryCounterInfo, counter: int, sequence: c104.UInt5, quality: c104.BinaryCounterQuality = c104.BinaryCounterQuality(), recorded_at: c104.DateTime = None) -> None

create a new short measurement info

Parameters
----------
counter: int
    Counter value
sequence: c104.UInt5
    Counter info sequence number
quality: c104.BinaryCounterQuality
    Binary counter quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> counter_info = c104.BinaryCounterInfo(counter=2345, sequence=c104.UInt5(35), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "counter"_a, "sequence"_a = LimitedUInt5(0),
          "quality"_a = BinaryCounterQuality::None,
          "recorded_at"_a = py::none())
      .def_property_readonly("counter", &BinaryCounterInfo::getCounter,
                             "int: the actual counter-value (read-only)")
      .def_property_readonly(
          "sequence", &BinaryCounterInfo::getSequence,
          "c104.UInt5: the counter sequence number (read-only)")
      .def_property_readonly(
          "value", &BinaryCounterInfo::getValue,
          R"def(int: references property ``counter`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly(
          "quality", &BinaryCounterInfo::getQuality,
          R"def(c104.BinaryCounterQuality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &BinaryCounterInfo::toString);

  py::class_<ProtectionEquipmentEventInfo, IInformation,
             std::shared_ptr<ProtectionEquipmentEventInfo>>(
      m, "ProtectionEventInfo",
      "This class represents all specific protection equipment single event "
      "point information")
      .def(
          py::init(&ProtectionEquipmentEventInfo::create),
          R"def(__init__(self: c104.ProtectionEventInfo, state: c104.EventState | int, elapsed_ms: c104.UInt16|int, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new event info raised by protection equipment

Parameters
----------
state: c104.EventState | int
    State of the event
elapsed_ms: c104.UInt16 | int
    Time in milliseconds elapsed
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> single_event = c104.ProtectionEventInfo(state=c104.EventState.ON, elapsed_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "state"_a, "elapsed_ms"_a = LimitedUInt16(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("state", &ProtectionEquipmentEventInfo::getState,
                             "c104.EventState: the state (read-only)")
      .def_property_readonly(
          "value", &ProtectionEquipmentEventInfo::getValue,
          R"def(c104.EventState: references property ``state`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality",
                             &ProtectionEquipmentEventInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def_property_readonly(
          "elapsed_ms", &ProtectionEquipmentEventInfo::getElapsed_ms,
          "int: the elapsed time in milliseconds (read-only)")
      .def("__repr__", &ProtectionEquipmentEventInfo::toString);

  py::class_<ProtectionEquipmentStartEventsInfo, IInformation,
             std::shared_ptr<ProtectionEquipmentStartEventsInfo>>(
      m, "ProtectionStartInfo",
      "This class represents all specific protection equipment packed start "
      "events point information")
      .def(
          py::init(&ProtectionEquipmentStartEventsInfo::create),
          R"def(__init__(self: c104.ProtectionStartInfo, events: c104.StartEvents | int, relay_duration_ms: c104.UInt16|int, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new packed event start info raised by protection equipment

Parameters
----------
events: c104.StartEvents | int
    Set of start events
relay_duration_ms: c104.UInt16 | int
    Time in milliseconds of relay duration
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> start_events = c104.ProtectionStartInfo(events=c104.StartEvents.ON, relay_duration_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "events"_a, "relay_duration_ms"_a = LimitedUInt16(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly("events",
                             &ProtectionEquipmentStartEventsInfo::getEvents,
                             "c104.StartEvents: the started events (read-only)")
      .def_property_readonly(
          "relay_duration_ms",
          &ProtectionEquipmentStartEventsInfo::getRelayDuration_ms,
          "int: the relay duration information (read-only)")
      .def_property_readonly(
          "value", &ProtectionEquipmentStartEventsInfo::getValue,
          R"def(c104.StartEvents: references property ``events`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality",
                             &ProtectionEquipmentStartEventsInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &ProtectionEquipmentStartEventsInfo::toString);

  py::class_<ProtectionEquipmentOutputCircuitInfo, IInformation,
             std::shared_ptr<ProtectionEquipmentOutputCircuitInfo>>(
      m, "ProtectionCircuitInfo",
      "This class represents all specific protection equipment output circuit "
      "point information")
      .def(
          py::init(&ProtectionEquipmentOutputCircuitInfo::create),
          R"def(__init__(self: c104.ProtectionCircuitInfo, circuits: c104.OutputCircuits | int, relay_operating_ms: c104.UInt16|int, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new output circuits info raised by protection equipment

Parameters
----------
circuits: c104.OutputCircuits | int
    Set of output circuits
relay_operating_ms: c104.UInt16 | int
    Time in milliseconds of relay operation
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> output_circuits = c104.ProtectionCircuitInfo(events=c104.OutputCircuits.PhaseL1|c104.OutputCircuits.PhaseL2, relay_operating_ms=c104.UInt16(35000), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "events"_a, "relay_duration_ms"_a = LimitedUInt16(0),
          "quality"_a = Quality::None, "recorded_at"_a = py::none())
      .def_property_readonly(
          "circuits", &ProtectionEquipmentOutputCircuitInfo::getCircuits,
          "c104.OutputCircuits: the started events (read-only)")
      .def_property_readonly(
          "relay_operating_ms",
          &ProtectionEquipmentOutputCircuitInfo::getRelayOperating_ms,
          "int: the relay operation duration information (read-only)")
      .def_property_readonly(
          "value", &ProtectionEquipmentOutputCircuitInfo::getValue,
          R"def(c104.OutputCircuits: references property ``circuits`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality",
                             &ProtectionEquipmentOutputCircuitInfo::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &ProtectionEquipmentOutputCircuitInfo::toString);

  py::class_<StatusWithChangeDetection, IInformation,
             std::shared_ptr<StatusWithChangeDetection>>(
      m, "StatusAndChanged",
      "This class represents all specific packed status point information with "
      "change detection")
      .def(
          py::init(&StatusWithChangeDetection::create),
          R"def(__init__(self: c104.StatusAndChanged, status: c104.PackedSingle | int, changed: c104.PackedSingle|int, quality: c104.Quality = c104.Quality(), recorded_at: c104.DateTime = None) -> None

create a new event info raised by protection equipment

Parameters
----------
status: c104.PackedSingle | int
    Set of current single values
changed: c104.PackedSingle | int
    Set of changed single values
quality: c104.Quality
    Quality information
recorded_at: c104.DateTime, optional
    Timestamp contained in the protocol message, or None if the protocol message type does not contain a timestamp.

Example
-------
>>> status_and_changed = c104.StatusAndChanged(status=c104.PackedSingle.I0|c104.PackedSingle.I5, changed=c104.PackedSingle(15), quality=c104.Quality.Invalid, recorded_at=datetime.datetime.now(datetime.utc))
)def",
          "status"_a, "changed"_a = FieldSet16(0), "quality"_a = Quality::None,
          "recorded_at"_a = py::none())
      .def_property_readonly(
          "status", &StatusWithChangeDetection::getStatus,
          "c104.PackedSingle: the current status (read-only)")
      .def_property_readonly(
          "changed", &StatusWithChangeDetection::getChanged,
          "c104.PackedSingle: the changed information (read-only)")
      .def_property_readonly(
          "value", &StatusWithChangeDetection::getValue,
          R"def(c104.PackedSingle: references property ``status`` (read-only)

The setter is available via point.value=xyz)def")
      .def_property_readonly("quality", &StatusWithChangeDetection::getQuality,
                             R"def(c104.Quality: the quality (read-only)

The setter is available via point.quality=xyz)def")
      .def("__repr__", &StatusWithChangeDetection::toString);
}
