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
 * @file DataPointPy.cpp
 * @brief python binding for DataPoint class
 *
 * @package iec104-python
 * @namespace Object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "object/DataPoint.h"
#include "object/Station.h"

using namespace pybind11::literals;

void init_object_datapoint(py::module_ &m) {

  py::class_<Object::DataPoint, std::shared_ptr<Object::DataPoint>>(
      m, "Point",
      "This class represents command and measurement data point of a station "
      "and provides access to structured properties of points")
      .def_property_readonly(
          "station", &Object::DataPoint::getStation,
          "c104.Station | None : parent Station object (read-only)")
      .def_property_readonly("io_address",
                             &Object::DataPoint::getInformationObjectAddress,
                             "int : information object address (read-only)")
      .def_property_readonly("type", &Object::DataPoint::getType,
                             "c104.Type : data related IEC60870 message type "
                             "identifier (read-only)")
      .def_property("related_io_address",
                    &Object::DataPoint::getRelatedInformationObjectAddress,
                    &Object::DataPoint::setRelatedInformationObjectAddress,
                    "int | None : io_address of a related monitoring "
                    "point or None")
      .def_property(
          "related_io_autoreturn",
          &Object::DataPoint::getRelatedInformationObjectAutoReturn,
          &Object::DataPoint::setRelatedInformationObjectAutoReturn,
          "bool: automatic transmission of return info remote messages for "
          "related point on incoming client command (only for control points)")
      .def_property("command_mode", &Object::DataPoint::getCommandMode,
                    &Object::DataPoint::setCommandMode,
                    "c104.CommandMode : command transmission mode (direct or "
                    "select-and-execute)",
                    py::return_value_policy::copy)
      .def_property_readonly("selected_by",
                             &Object::DataPoint::getSelectedByOriginatorAddress,
                             "int | None : originator address (0-255) or None")
      .def_property("report_ms", &Object::DataPoint::getReportInterval_ms,
                    &Object::DataPoint::setReportInterval_ms,
                    "int : interval in milliseconds between periodic "
                    "transmission, 0 = no periodic transmission")
      .def_property_readonly("timer_ms",
                             &Object::DataPoint::getTimerInterval_ms,
                             "int : interval in milliseconds between timer "
                             "callbacks, 0 = no periodic transmission")
      .def_property("info", &Object::DataPoint::getInfo,
                    &Object::DataPoint::setInfo,
                    "c104.Information : current information",
                    py::return_value_policy::automatic)
      .def_property(
          "groups", &Object::DataPoint::getGroups,
          &Object::DataPoint::setGroups,
          "tuple[c104.Point|None] : tuple of points that belong to the group",
          py::return_value_policy::copy)
      .def_property(
          "value", &Object::DataPoint::getValue, &Object::DataPoint::setValue,
          "typing.Union[None, bool, c104.Double, c104.Step, c104.Int7, "
          "c104.Int16, int, c104.Byte32, c104.NormalizedFloat, float, "
          "c104.EventState, c104.StartEvents, c104.OutputCircuits, "
          "c104.PackedSingle] : the primary information value (this is just a "
          "shortcut to "
          "point.info.value)",
          py::return_value_policy::copy)
      .def_property(
          "quality", &Object::DataPoint::getQuality,
          &Object::DataPoint::setQuality,
          "typing.Union[None, c104.Quality, c104.BinaryCounterQuality] : "
          "the primary quality value (this is just a shortcut to "
          "point.info.quality)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "processed_at", &Object::DataPoint::getProcessedAt,
          "c104.DateTime : timestamp with milliseconds of last local "
          "information processing "
          "(read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("recorded_at", &Object::DataPoint::getRecordedAt,
                             "c104.DateTime | None : timestamp with "
                             "milliseconds transported with the "
                             "value "
                             "itself or None (read-only)",
                             py::return_value_policy::copy)
      .def(
          "on_receive", &Object::DataPoint::setOnReceiveCallback,
          R"def(on_receive(self: c104.Point, callable: collections.abc.Callable[[c104.Point, c104.Information, c104.IncomingMessage], c104.ResponseState]) -> None

set python callback that will be executed on every incoming message
this can be either a command or an monitoring message

Parameters
----------
callable: collections.abc.Callable[[c104.Point, c104.Information, c104.IncomingMessage], c104.ResponseState]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance
previous_info: c104.Information
    Information object containing the state of the point before the command took effect
message: c104.IncomingMessage
    new command message

Callable Returns
----------------
c104.ResponseState
    send command SUCCESS or FAILURE response

Example
-------
>>> def on_setpoint_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
>>>     print("SV] {0} SETPOINT COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_info, message.cot, point.quality))
>>>     if point.related_io_address:
>>>         print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
>>>         related_point = sv_station_2.get_point(point.related_io_address)
>>>         if related_point:
>>>             print("SV] -> RELATED POINT VALUE UPDATE")
>>>             related_point.value = point.value
>>>         else:
>>>             print("SV] -> RELATED POINT NOT FOUND!")
>>>     return c104.ResponseState.SUCCESS
>>>
>>> sv_measurement_point = sv_station_2.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
>>> sv_measurement_point.value = 12.34
>>> sv_command_point = sv_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=sv_measurement_point.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
>>> sv_command_point.on_receive(callable=on_setpoint_command)
)def",
          "callable"_a)
      .def(
          "on_before_read", &Object::DataPoint::setOnBeforeReadCallback,
          R"def(on_before_read(self: c104.Point, callable: collections.abc.Callable[[c104.Point], None]) -> None

set python callback that will be called on incoming interrogation or read commands to support polling

Parameters
----------
callable: collections.abc.Callable[[c104.Point], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly, parent station reference is invalid or function is called from client context

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance

Callable Returns
----------------
None

Example
-------
>>> def on_before_read_steppoint(point: c104.Point) -> None:
>>>     print("SV] {0} READ COMMAND on IOA: {1}".format(point.type, point.io_address))
>>>     point.value = random.randint(-64,63)  # import random
>>>
>>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
>>> step_point.on_before_read(callable=on_before_read_steppoint)
)def",
          "callable"_a)
      .def(
          "on_before_auto_transmit",
          &Object::DataPoint::setOnBeforeAutoTransmitCallback,
          R"def(on_before_auto_transmit(self: c104.Point, callable: collections.abc.Callable[[c104.Point], None]) -> None

set python callback that will be called before server reports a measured value interval-based

Parameters
----------
callable: collections.abc.Callable[[c104.Point], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly, parent station reference is invalid or function is called from client context

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance

Callable Returns
----------------
None

Warning
-------
The difference between **on_before_read** and **on_before_auto_transmit** is the calling context.
**on_before_read** is called when a client sends a command to report a point (interrogation or read).
**on_before_auto_transmit** is called when the server reports a measured value interval-based.

Example
-------
>>> def on_before_auto_transmit_step(point: c104.Point) -> None:
>>>     print("SV] {0} PERIODIC TRANSMIT on IOA: {1}".format(point.type, point.io_address))
>>>     point.value = c104.Int7(random.randint(-64,63))  # import random
>>>
>>> step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1, report_ms=2000)
>>> step_point.on_before_auto_transmit(callable=on_before_auto_transmit_step)
)def",
          "callable"_a)
      .def(
          "on_timer", &Object::DataPoint::setOnTimerCallback,
          R"def(on_timer(self: c104.Point, callable: collections.abc.Callable[[c104.Point], None], int) -> None

set python callback that will be called in a fixed delay (timer_ms)

Parameters
----------
callable: collections.abc.Callable[[c104.Point], None]
    callback function reference
interval_ms: int
    interval between two callback executions in milliseconds

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly, parent station reference is invalid or function is called from client context

**Callable signature**

Callable Parameters
-------------------
point: c104.Point
    point instance
interval_ms: int
    fixed delay between timer callback execution, default: 0, min: 50

Callable Returns
----------------
None

Example
-------
>>> def on_timer(point: c104.Point) -> None:
>>>     print("SV] {0} TIMER on IOA: {1}".format(point.type, point.io_address))
>>>     point.value = random.randint(-64,63)  # import random
>>>
>>> nv_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ME_TD_1)
>>> nv_point.on_timer(callable=on_timer, interval_ms=1000)
)def",
          "callable"_a, "interval_ms"_a = 0)
      .def("read", &Object::DataPoint::read,
           R"def(read(self: c104.Point) -> bool

send read command

Returns
-------
bool
    True if the command was successfully accepted by the server, otherwise False

Raises
------
ValueError
    parent station or connection reference is invalid or called from remote terminal unit (server) context

Example
-------
>>> if cl_step_point.read():
>>>     print("read command successful")
)def",
           py::return_value_policy::copy)
      .def("transmit", &Object::DataPoint::transmit,
           R"def(transmit(self: c104.Point, cause: c104.Cot) -> bool

**Server-side point**
report a measurement value to connected clients

**Client-side point**
send the command point to the server

Parameters
----------
cause: c104.Cot
    cause of the transmission

Raises
------
ValueError
    parent station, server or connection reference is invalid

Returns
-------
bool
    True if the command was successfully send (server-side) or accepted by the server (client-side), otherwise False

Example
-------
>>> sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)
>>> cl_single_command_point.transmit(cause=c104.Cot.ACTIVATION)
)def",
           "cause"_a, py::return_value_policy::copy)
      .def("__repr__", &Object::DataPoint::toString);
}
