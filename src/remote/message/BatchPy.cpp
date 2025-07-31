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
 * @file BatchPy.cpp
 * @brief python binding for Batch class
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "module/Tuple.h"
#include "object/DataPoint.h"
#include "remote/message/Batch.h"

using namespace pybind11::literals;

void init_remote_batch(py::module_ &m) {

  py::class_<Remote::Message::Batch, std::shared_ptr<Remote::Message::Batch>>(
      m, "Batch",
      "This class represents a batch of outgoing monitoring messages of the "
      "same station "
      "and type")
      .def(
          py::init(&Remote::Message::Batch::create),
          R"def(__init__(self, cause: c104.Cot, points: list[c104.Point] | None = None) -> None

create a new batch of monitoring messages of the same station and the same type

Parameters
----------
cause: c104.Cot
    what caused the transmission of the monitoring data
points: list[c104.Point], optional
    initial list of points

Raises
------
ValueError
    if one point in the list is not compatible with the others

Example
-------
>>> batch = c104.Batch(cause=c104.Cot.SPONTANEOUS, points=[point1, point2, point3])
)def",
          "cause"_a, "points"_a = py::none())
      .def_property_readonly(
          "type", &Remote::Message::Batch::getType,
          "c104.Type: IEC60870 message type identifier (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("common_address",
                             &Remote::Message::Batch::getCommonAddress,
                             "int: common address (1-65534) (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("originator_address",
                             &Remote::Message::Batch::getOriginatorAddress,
                             "int: originator address (0-255) (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("cot",
                             &Remote::Message::Batch::getCauseOfTransmission,
                             "c104.Cot: cause of transmission (read-only)",
                             py::return_value_policy::copy)
      .def_property_readonly("is_test", &Remote::Message::Batch::isTest,
                             "bool: test if test flag is set (read-only)")
      .def_property_readonly("is_sequence", &Remote::Message::Batch::isSequence,
                             "bool: test if sequence flag is set (read-only)")
      .def_property_readonly("is_negative", &Remote::Message::Batch::isNegative,
                             "bool: test if negative flag is set (read-only)")
      .def_property_readonly(
          "number_of_objects", &Remote::Message::Batch::getNumberOfObjects,
          "int: represents the number of information objects (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("has_points", &Remote::Message::Batch::hasPoints,
                             "bool: test if batch contains points (read-only)")
      .def_property_readonly(
          "points",
          [](const Remote::Message::Batch &b) {
            return Module::vector_to_tuple(b.getPoints());
          },
          "tuple[c104.Point]: list of contained points (read-only)")
      .def("add_point", &Remote::Message::Batch::addPoint,
           R"def(add_point(self: c104.Batch, point: c104.Point) -> None

add a new point to this Batch

Parameters
----------
point: c104.Point
    to be added point

Returns
-------
None

Raises
------
ValueError
    if point is not compatible with the batch or if it is already in the batch

Example
-------
>>> my_batch.add_point(my_point)
)def",
           "point"_a)
      .def("__repr__", &Remote::Message::Batch::toString);
  ;
}
