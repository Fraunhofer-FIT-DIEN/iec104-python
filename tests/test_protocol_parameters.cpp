/**
 * Copyright 2026 Fraunhofer Institute for Applied Information Technology FIT
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
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 * @file test_protocol_parameters.cpp
 * @brief Verifies the ProtocolParameters binding (sCS104_APCIParameters) both
 * on the raw C++ struct and through the embedded-interpreter pybind11 surface.
 *
 * Design notes:
 * - ProtocolParameters maps 1:1 to the lib60870 sCS104_APCIParameters struct
 *   whose six fields (k, w, t0, t1, t2, t3) are exposed in python.cpp as
 *   readwrite attributes: send_window_size=k, receive_window_size=w,
 *   connection_timeout=t0, message_timeout=t1, confirm_interval=t2 and
 *   keep_alive_interval=t3.
 * - Both Server::getParameters() and Remote::Connection::getParameters()
 *   return the underlying CS104_APCIParameters pointer, so reads and writes
 *   alias the live struct.
 * - GIL: mirrors tests/main.cpp - the embedded interpreter is created once in
 *   main.cpp's py::scoped_interpreter; this file only imports the module and
 * never spawns a second interpreter.
 */

#include <catch2/catch_test_macros.hpp>
#include <pybind11/pybind11.h>

#include <sstream>
#include <string>

#include "Client.h"
#include "Server.h"
#include "remote/Connection.h"

namespace py = pybind11;

namespace {

// The six field values used to exercise get/set. Distinct from the lib60870
// defaults so a read-back proves the write actually landed.
constexpr int kSendWindow = 8;
constexpr int kReceiveWindow = 12;
constexpr int kConnectionTimeout = 30;
constexpr int kMessageTimeout = 10;
constexpr int kConfirmInterval = 15;
constexpr int kKeepAliveInterval = 20;

// Set all six fields on a raw lib60870 APCIParameters struct.
void setAllFields(sCS104_APCIParameters &p) {
  p.k = kSendWindow;
  p.w = kReceiveWindow;
  p.t0 = kConnectionTimeout;
  p.t1 = kMessageTimeout;
  p.t2 = kConfirmInterval;
  p.t3 = kKeepAliveInterval;
}

// Assert every field on a raw lib60870 APCIParameters struct equals the
// expected value written by setAllFields().
void requireAllFields(const sCS104_APCIParameters &p) {
  REQUIRE(p.k == kSendWindow);
  REQUIRE(p.w == kReceiveWindow);
  REQUIRE(p.t0 == kConnectionTimeout);
  REQUIRE(p.t1 == kMessageTimeout);
  REQUIRE(p.t2 == kConfirmInterval);
  REQUIRE(p.t3 == kKeepAliveInterval);
}

} // namespace

// ---------------------------------------------------------------------------
// Direct C++ struct field access on Server / Connection protocol parameters
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: raw C++ struct fields on Server",
          "[protocol-parameters][struct]") {
  auto server = Server::create("127.0.0.1", 24212);
  CS104_APCIParameters params = server->getParameters();
  REQUIRE(params != nullptr);

  // Write via the returned pointer, read back the aliased struct.
  setAllFields(*params);
  requireAllFields(*params);
}

TEST_CASE("ProtocolParameters: raw C++ struct fields on Connection",
          "[protocol-parameters][struct]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 24213);
  REQUIRE(connection != nullptr);

  CS104_APCIParameters params = connection->getParameters();
  REQUIRE(params != nullptr);

  setAllFields(*params);
  requireAllFields(*params);
}

// ---------------------------------------------------------------------------
// Embedded interpreter: c104.Server.protocol_parameters
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: embedded server get/set",
          "[protocol-parameters]") {
  auto c104 = py::module_::import("c104");
  auto Server = c104.attr("Server");

  py::object server = Server("127.0.0.1", 24214, py::arg("tick_rate_ms") = 100);
  py::object pp = server.attr("protocol_parameters");

  // Set each of the six parameters.
  pp.attr("send_window_size") = kSendWindow;
  pp.attr("receive_window_size") = kReceiveWindow;
  pp.attr("connection_timeout") = kConnectionTimeout;
  pp.attr("message_timeout") = kMessageTimeout;
  pp.attr("confirm_interval") = kConfirmInterval;
  pp.attr("keep_alive_interval") = kKeepAliveInterval;

  // Verify they are read back correctly.
  REQUIRE(pp.attr("send_window_size").cast<int>() == kSendWindow);
  REQUIRE(pp.attr("receive_window_size").cast<int>() == kReceiveWindow);
  REQUIRE(pp.attr("connection_timeout").cast<int>() == kConnectionTimeout);
  REQUIRE(pp.attr("message_timeout").cast<int>() == kMessageTimeout);
  REQUIRE(pp.attr("confirm_interval").cast<int>() == kConfirmInterval);
  REQUIRE(pp.attr("keep_alive_interval").cast<int>() == kKeepAliveInterval);

  server.attr("stop")();
}

// ---------------------------------------------------------------------------
// Embedded interpreter: c104.Connection.protocol_parameters
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: embedded connection get/set",
          "[protocol-parameters]") {
  auto c104 = py::module_::import("c104");
  auto Client = c104.attr("Client");

  py::object client = Client(py::arg("tick_rate_ms") = 100);
  py::object connection = client.attr("add_connection")("127.0.0.1", 24215);
  REQUIRE(connection.is_none() == false);
  REQUIRE(py::hasattr(connection, "protocol_parameters"));

  py::object pp = connection.attr("protocol_parameters");

  pp.attr("send_window_size") = kSendWindow;
  pp.attr("receive_window_size") = kReceiveWindow;
  pp.attr("connection_timeout") = kConnectionTimeout;
  pp.attr("message_timeout") = kMessageTimeout;
  pp.attr("confirm_interval") = kConfirmInterval;
  pp.attr("keep_alive_interval") = kKeepAliveInterval;

  REQUIRE(pp.attr("send_window_size").cast<int>() == kSendWindow);
  REQUIRE(pp.attr("receive_window_size").cast<int>() == kReceiveWindow);
  REQUIRE(pp.attr("connection_timeout").cast<int>() == kConnectionTimeout);
  REQUIRE(pp.attr("message_timeout").cast<int>() == kMessageTimeout);
  REQUIRE(pp.attr("confirm_interval").cast<int>() == kConfirmInterval);
  REQUIRE(pp.attr("keep_alive_interval").cast<int>() == kKeepAliveInterval);

  client.attr("stop")();
}

// ---------------------------------------------------------------------------
// __str__ / __repr__ containment of all six values
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: str and repr contain all six values",
          "[protocol-parameters]") {
  auto c104 = py::module_::import("c104");
  auto Server = c104.attr("Server");

  py::object server = Server("127.0.0.1", 24216, py::arg("tick_rate_ms") = 100);
  py::object pp = server.attr("protocol_parameters");

  pp.attr("send_window_size") = kSendWindow;
  pp.attr("receive_window_size") = kReceiveWindow;
  pp.attr("connection_timeout") = kConnectionTimeout;
  pp.attr("message_timeout") = kMessageTimeout;
  pp.attr("confirm_interval") = kConfirmInterval;
  pp.attr("keep_alive_interval") = kKeepAliveInterval;

  auto str_value = py::str(pp).cast<std::string>();
  auto repr_value = py::repr(pp).cast<std::string>();

  // Each of the six values must appear in both string forms.
  for (const std::string &needle :
       {std::to_string(kSendWindow), std::to_string(kReceiveWindow),
        std::to_string(kConnectionTimeout), std::to_string(kMessageTimeout),
        std::to_string(kConfirmInterval), std::to_string(kKeepAliveInterval)}) {
    INFO("needle '" << needle << "' not found in __str__: " << str_value);
    REQUIRE(str_value.find(needle) != std::string::npos);
    INFO("needle '" << needle << "' not found in __repr__: " << repr_value);
    REQUIRE(repr_value.find(needle) != std::string::npos);
  }

  // Field names must be present in both forms.
  REQUIRE(str_value.find("send_window_size") != std::string::npos);
  REQUIRE(str_value.find("receive_window_size") != std::string::npos);
  REQUIRE(str_value.find("connection_timeout") != std::string::npos);
  REQUIRE(str_value.find("message_timeout") != std::string::npos);
  REQUIRE(str_value.find("confirm_interval") != std::string::npos);
  REQUIRE(str_value.find("keep_alive_interval") != std::string::npos);
  REQUIRE(repr_value.find("send_window_size") != std::string::npos);
  REQUIRE(repr_value.find("receive_window_size") != std::string::npos);
  REQUIRE(repr_value.find("connection_timeout") != std::string::npos);
  REQUIRE(repr_value.find("message_timeout") != std::string::npos);
  REQUIRE(repr_value.find("confirm_interval") != std::string::npos);
  REQUIRE(repr_value.find("keep_alive_interval") != std::string::npos);

  server.attr("stop")();
}
