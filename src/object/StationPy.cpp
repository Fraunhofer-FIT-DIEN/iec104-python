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
 * @file StationPy.cpp
 * @brief python binding for Station class
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

#include "Server.h"
#include "module/Tuple.h"
#include "object/Station.h"
#include "remote/Connection.h"

using namespace pybind11::literals;

void init_object_station(py::module_ &m) {

  py::class_<Object::Station, std::shared_ptr<Object::Station>>(
      m, "Station",
      "This class represents local or remote stations and provides access to "
      "meta information and containing points")
      .def_property_readonly("server", &Object::Station::getServer,
                             "c104.Server | None : parent Server of "
                             "local station (read-only)")
      .def_property_readonly("connection", &Object::Station::getConnection,
                             "c104.Connection | None : parent "
                             "Connection of non-local station (read-only)")
      .def_property_readonly(
          "common_address", &Object::Station::getCommonAddress,
          "int: common address of this station (1-65534) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_local", &Object::Station::isLocal,
                             "bool: test if station is local (has sever) or "
                             "remote (has connection) one (read-only)")
      .def_property(
          "daylight_saving_time", &Object::Station::isDaylightSavingTime,
          &Object::Station::setDaylightSavingTime,
          R"def(bool: if timestamps recorded at this station are in daylight saving time

Changing this flag will modify the timezone_offset of the station by +-3600 seconds!

The daylight_saving_time (aka summertime flag) will add an additional hour on top of timezone_offset property.

The use of the summertime (SU) flag is optional but generally discouraged - use UTC instead.
A timestamp with the SU flag set represents the identical time value as a timestamp with the SU flag unset,
but with the displayed value shifted exactly one hour earlier.
This may help in assigning the correct hour to information objects generated during the first hour after
transitioning from daylight savings time (summertime) to standard time.
)def")
      .def_property(
          "timezone_offset", &Object::Station::getTimeZoneOffset,
          &Object::Station::setTimeZoneOffset,
          "datetime.timedelta: timezone offset for protocol timestamps")
      .def_property("auto_time_substituted",
                    &Object::Station::isAutoTimeSubstituted,
                    &Object::Station::setAutoTimeSubstituted,
                    "bool: flagging of auto-assigned reported_at timestamps as "
                    "substituted"
                    "flagged as substituted")
      .def_property_readonly(
          "has_points", &Object::Station::hasPoints,
          "bool: test if station has at least one point (read-only)")
      .def_property_readonly(
          "points",
          [](const Object::Station &s) {
            return Module::vector_to_tuple(s.getPoints());
          },
          "tuple[c104.Point]: list of all Point objects (read-only)")
      .def(
          "get_point", &Object::Station::getPoint,
          R"def(get_point(self: c104.Station, io_address: int) -> c104.Point | None

get a point object via information object address

Parameters
----------
io_address: int
    point information object address (value between 0 and 16777215)

Returns
-------
c104.Point, optional
    point object, if found, else None

Example
-------
>>> point_11 = my_station.get_point(io_address=11)
)def",
          "io_address"_a)
      .def(
          "add_point", &Object::Station::addPoint,
          R"def(add_point(self: c104.Station, io_address: int, type: c104.Type, report_ms: int = 0, related_io_address: int = None, related_io_autoreturn: bool = False, command_mode: c104.CommandMode = c104.CommandMode.DIRECT) -> c104.Point | None

add a new point to this station and return the new point object

Parameters
----------
io_address: int
    point information object address (value between 0 and 16777215)
type: c104.Type
    point information type
report_ms: int
    automatic reporting interval in milliseconds (monitoring points server-sided only), 0 = disabled
related_io_address: int, optional
    related monitoring point identified by information object address, that should be auto transmitted on incoming client command (for control points server-sided only)
related_io_autoreturn: bool
    automatic reporting interval in milliseconds (for control points server-sided only)
command_mode: c104.CommandMode
    command transmission mode (direct or select-and-execute)

Returns
-------
c104.Station, optional
    station object, if station was added, else None

Raises
------
ValueError
    io_address or type is invalid
ValueError
    report_ms, related_io_address or related_auto_return is set, but type is not a monitoring type
ValueError
    related_auto_return is set, but related_io_address is not set
ValueError
    related_auto_return is set, but type is not a control type

Example
-------
>>> point_1 = sv_station_1.add_point(common_address=15, type=c104.Type.M_ME_NC_1)
>>> point_2 = sv_station_1.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
>>> point_3 = sv_station_1.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=point_2.io_address, related_io_autoreturn=True, command_mode=c104.CommandMode.SELECT_AND_EXECUTE)
)def",
          "io_address"_a, "type"_a, "report_ms"_a = 0,
          "related_io_address"_a = py::none(),
          "related_io_autoreturn"_a = false, "command_mode"_a = DIRECT_COMMAND)
      .def("remove_point", &Object::Station::removePoint,
           R"def(remove_point(self: c104.Station, io_address: int) -> bool

remove an existing point from this station

Parameters
----------
io_address: int
    point information object address (value between 0 and 16777215)

Returns
-------
bool
    True if the point was successfully removed, otherwise False.

Example
-------
>>> sv_station_1.remove_point(io_address=17)
)def",
           "io_address"_a)
      .def(
          "signal_initialized", &Object::Station::sendEndOfInitialization,
          R"def(signal_initialized(self: c104.Station, cause: c104.Coi) -> None

signal end of initialization for this station to connected clients

Parameters
----------
cause: c104.Coi
    what caused the (re-)initialization procedure

Returns
-------
None

Example
-------
>>> my_station.signal_initialized(cause=c104.Coi.REMOTE_RESET)
)def",
          "cause"_a)
      .def(
          "get_group",
          [](const Object::Station &s, const size_t group_id) {
            return Module::vector_to_tuple(s.getGroup(group_id));
          },
          R"def(get_group(self: c104.Station, group_id: int) -> tuple[c104.Point]

get a list of points that belong to the group (0=all points) targetable in qualifiers like interrogation commands

Parameters
----------
group_id: int
    interrogation group index (value between 1 and 16, 0 = all points)

Returns
-------
tuple[c104.Point]
    list of points that are member of this group

Example
-------
>>> group_1 = my_station.get_group(group_id=1)
)def",
          "io_address"_a)
      .def("__repr__", &Object::Station::toString);
}
