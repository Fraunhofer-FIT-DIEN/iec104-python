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
 * @file DateTimePy.cpp
 * @brief python binding for DateTime class
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

#include "object/DateTime.h"

using namespace pybind11::literals;

void init_object_datetime(py::module_ &m) {

  py::class_<Object::DateTime, std::shared_ptr<Object::DateTime>>(
      m, "DateTime",
      "This class represents date time objects with additional flags.")
      .def(
          py::init<const py::object &, const bool, const bool, const bool>(),
          R"def(__init__(self: c104.DateTime, value: datetime.datetime, substituted: bool = False, invalid: bool = False, daylight_saving_time: bool = False) -> None

create a new DateTime

Parameters
----------
value: datetime.datetime
    datetime value with optional timezone information
substituted: bool, optional
    flag as to whether the datetime value is substituted
invalid: bool, optional
    flag as to whether the datetime value is invalid
daylight_saving_time: bool, optional
    flag as to whether the datetime value is daylight saving time, adds additional 1 hour to the datetime value

Example
-------
>>> dt = c104.DateTime(datetime.datetime.now(datetime.timezone(datetime.timedelta(seconds=14400))), daylight_saving_time=True)
)def",
          "value"_a, "substituted"_a = false, "invalid"_a = false,
          "daylight_saving_time"_a = false)
      .def_static(
          "now",
          []() {
            const auto datetime = py::module::import("datetime");
            const auto now =
                datetime.attr("datetime")
                    .attr("now")(datetime.attr("timezone").attr("utc"));
            return Object::DateTime(now, false, false, false);
          },
          R"def(now() -> c104.DateTime

create a new DateTime object with current date and time

Returns
-------
c104.DateTime
    current date and time object

Example
-------
>>> dt = c104.DateTime.now()
)def")
      .def_property_readonly("value", &Object::DateTime::toPyDateTime,
                             "datetime.datetime: timezone aware datetime "
                             "object for this timestamp (read-only)")
      .def_property_readonly("readonly", &Object::DateTime::isReadonly,
                             "bool: if this timestamp is readonly (read-only)")
      .def_property("substituted", &Object::DateTime::isSubstituted,
                    &Object::DateTime::setSubstituted,
                    "bool: if this timestamp was flagged as substituted")
      .def_property("invalid", &Object::DateTime::isInvalid,
                    &Object::DateTime::setInvalid,
                    "bool: if this timestamp was flagged as invalid")
      .def_property(
          "daylight_saving_time", &Object::DateTime::isDaylightSavingTime,
          &Object::DateTime::setDaylightSavingTime,
          R"def(bool: if this timestamp was recorded in daylight saving time

Changing this flag will modify the timestamp send by +1 hour!

The summertime offset will be added on top of timezone offset provided with the datetime.

The use of the summertime (SU) flag is optional but generally discouraged - use UTC instead.
A timestamp with the SU flag set represents the identical time value as a timestamp with the SU flag unset,
but with the displayed value shifted exactly one hour earlier.
This may help in assigning the correct hour to information objects generated during the first hour after
transitioning from daylight savings time (summertime) to standard time.
)def")
      .def_property_readonly("timezone_offset",
                             &Object::DateTime::getTimeZoneOffset,
                             "datetime.timedelta: timezone offset")
      .def("__repr__", &Object::DateTime::toString);
}
