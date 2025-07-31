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
 * @file IncomingMessagePy.cpp
 * @brief python binding for IncomingMessage class
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "object/information/IInformation.h"
#include "remote/message/IncomingMessage.h"

using namespace pybind11::literals;

/**
 * @brief Retrieves the raw byte data from an IncomingMessage as a Python bytes
 * object.
 *
 * This function accesses the raw byte data from the given IncomingMessage
 * object, calculates the size of the message including the header, and returns
 * the data encapsulated in a Python-compatible bytes object.
 *
 * @param message Pointer to an IncomingMessage object that contains the raw
 * byte data to retrieve.
 * @return py::bytes A Python bytes object containing the raw byte data from the
 * message.
 */
py::bytes
IncomingMessage_getRawBytes(const Remote::Message::IncomingMessage *message) {
  unsigned char *msg = message->getRawBytes();
  unsigned char msgSize = 2 + msg[1];

  return {reinterpret_cast<const char *>(msg), msgSize};
}

void init_remote_message(py::module_ &m) {

  py::class_<Remote::Message::IncomingMessage,
             std::shared_ptr<Remote::Message::IncomingMessage>>(
      m, "IncomingMessage",
      "This class represents incoming messages and provides access to "
      "structured properties interpreted from incoming messages")
      .def_property_readonly(
          "type", &Remote::Message::IncomingMessage::getType,
          "c104.Type: IEC60870 message type identifier (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "common_address", &Remote::Message::IncomingMessage::getCommonAddress,
          "int: common address (1-65534) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "originator_address",
          &Remote::Message::IncomingMessage::getOriginatorAddress,
          "int: originator address (0-255) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "io_address", &Remote::Message::IncomingMessage::getIOA,
          "int: information object address (0-16777215) (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "cot", &Remote::Message::IncomingMessage::getCauseOfTransmission,
          "c104.Cot: cause of transmission (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("info", &Remote::Message::IncomingMessage::getInfo,
                             "c104.Information: value (read-only)")
      .def_property_readonly("is_test",
                             &Remote::Message::IncomingMessage::isTest,
                             "bool: test if test flag is set (read-only)")
      .def_property_readonly("is_sequence",
                             &Remote::Message::IncomingMessage::isSequence,
                             "bool: test if sequence flag is set (read-only)")
      .def_property_readonly("is_negative",
                             &Remote::Message::IncomingMessage::isNegative,
                             "bool: test if negative flag is set (read-only)")
      .def_property_readonly("raw", &IncomingMessage_getRawBytes,
                             "bytes: raw ASDU message bytes (read-only)",
                             py::return_value_policy::take_ownership)
      .def_property_readonly(
          "raw_explain", &Remote::Message::IncomingMessage::getRawMessageString,
          "str: ASDU message bytes explained (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "number_of_object",
          &Remote::Message::IncomingMessage::getNumberOfObjects,
          "int: represents the number of information objects (read-only) "
          "(deprecated, use "
          "``number_of_objects`` instead)",
          py::return_value_policy::copy)
      .def_property_readonly(
          "number_of_objects",
          &Remote::Message::IncomingMessage::getNumberOfObjects,
          "int: represents the number of information objects contained in this "
          "message (read-only)",
          py::return_value_policy::copy)
      .def_property_readonly("is_select_command",
                             &Remote::Message::IncomingMessage::isSelectCommand,
                             "bool: test if message is a point command and has "
                             "select flag set (read-only)",
                             py::return_value_policy::copy)
      .def("first", &Remote::Message::IncomingMessage::first,
           R"def(first(self: c104.IncomingMessage) -> None

reset message information element pointer to first position

Returns
-------
None
)def")
      .def("next", &Remote::Message::IncomingMessage::next,
           R"def(next(self: c104.IncomingMessage) -> bool

move message information element pointer to next position, starting by first one

Returns
-------
bool
    True, if another information element exists, otherwise False
)def")
      .def("__repr__", &Remote::Message::IncomingMessage::toString);
}
