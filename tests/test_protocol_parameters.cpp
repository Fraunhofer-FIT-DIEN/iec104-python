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
 * @brief Verifies the ProtocolParameters view both on the C++ class and
 * through the embedded-interpreter pybind11 surface.
 *
 * Design notes:
 * - ProtocolParameters wraps the lib60870 sCS104_APCIParameters struct whose
 *   six fields (k, w, t0, t1, t2, t3) are exposed in python.cpp as validated
 *   properties: send_window_size=k, receive_window_size=w,
 *   connection_timeout=t0, message_timeout=t1, confirm_interval=t2 and
 *   keep_alive_interval=t3.
 * - Both Server::getParameters() and Remote::Connection::getParameters()
 *   return a view onto the live struct, so reads and writes alias it. The
 *   view keeps its owner alive.
 * - Writes are range checked (SECURITY.md M5: lib60870 uses k as malloc size
 *   and modulo divisor) and rejected while the owner is running.
 * - GIL: mirrors tests/main.cpp - the embedded interpreter is created once in
 *   main.cpp's py::scoped_interpreter; this file only imports the module and
 * never spawns a second interpreter.
 */

#include <catch2/catch_test_macros.hpp>
#include <pybind11/pybind11.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>

#include "Client.h"
#include "ProtocolParameters.h"
#include "Server.h"
#include "remote/Connection.h"

namespace py = pybind11;

namespace {

// The six field values used to exercise get/set. Distinct from the lib60870
// defaults (k=12, w=8, t0=10, t1=15, t2=10, t3=20) so a read-back proves the
// write actually landed. k is set before w, and k >= w holds throughout.
constexpr int kSendWindow = 16;
constexpr int kReceiveWindow = 10;
constexpr int kConnectionTimeout = 30;
constexpr int kMessageTimeout = 12;
constexpr int kConfirmInterval = 9;
constexpr int kKeepAliveInterval = 25;

// Set all six fields through the validated view.
void setAllFields(ProtocolParameters &p) {
  p.setSendWindowSize(kSendWindow);
  p.setReceiveWindowSize(kReceiveWindow);
  p.setConnectionTimeout(kConnectionTimeout);
  p.setMessageTimeout(kMessageTimeout);
  p.setConfirmInterval(kConfirmInterval);
  p.setKeepAliveInterval(kKeepAliveInterval);
}

// Assert every field on the raw lib60870 struct equals the expected value
// written by setAllFields(), proving the view aliases the live block.
void requireAllFields(const sCS104_APCIParameters &p) {
  REQUIRE(p.k == kSendWindow);
  REQUIRE(p.w == kReceiveWindow);
  REQUIRE(p.t0 == kConnectionTimeout);
  REQUIRE(p.t1 == kMessageTimeout);
  REQUIRE(p.t2 == kConfirmInterval);
  REQUIRE(p.t3 == kKeepAliveInterval);
}

// Set all six python properties in a valid order.
void setAllAttrs(py::object &pp) {
  pp.attr("send_window_size") = kSendWindow;
  pp.attr("receive_window_size") = kReceiveWindow;
  pp.attr("connection_timeout") = kConnectionTimeout;
  pp.attr("message_timeout") = kMessageTimeout;
  pp.attr("confirm_interval") = kConfirmInterval;
  pp.attr("keep_alive_interval") = kKeepAliveInterval;
}

void requireAllAttrs(const py::object &pp) {
  REQUIRE(pp.attr("send_window_size").cast<int>() == kSendWindow);
  REQUIRE(pp.attr("receive_window_size").cast<int>() == kReceiveWindow);
  REQUIRE(pp.attr("connection_timeout").cast<int>() == kConnectionTimeout);
  REQUIRE(pp.attr("message_timeout").cast<int>() == kMessageTimeout);
  REQUIRE(pp.attr("confirm_interval").cast<int>() == kConfirmInterval);
  REQUIRE(pp.attr("keep_alive_interval").cast<int>() == kKeepAliveInterval);
}

} // namespace

// ---------------------------------------------------------------------------
// C++ view on Server / Connection protocol parameters
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: view aliases the live struct on Server",
          "[protocol-parameters][struct]") {
  auto server = Server::create("127.0.0.1", 24212);
  auto params = server->getParameters();
  REQUIRE(params != nullptr);
  REQUIRE(params->getRaw() != nullptr);

  setAllFields(*params);
  requireAllFields(*params->getRaw());

  // a second view sees the same block
  auto again = server->getParameters();
  requireAllFields(*again->getRaw());
  REQUIRE(again->getRaw() == params->getRaw());
}

TEST_CASE("ProtocolParameters: view aliases the live struct on Connection",
          "[protocol-parameters][struct]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 24213);
  REQUIRE(connection != nullptr);

  auto params = connection->getParameters();
  REQUIRE(params != nullptr);
  REQUIRE(params->getRaw() != nullptr);

  setAllFields(*params);
  requireAllFields(*params->getRaw());
}

TEST_CASE("ProtocolParameters: view keeps its owner alive",
          "[protocol-parameters][lifetime]") {
  std::weak_ptr<Server> weak;
  std::shared_ptr<ProtocolParameters> params;
  {
    auto server = Server::create("127.0.0.1", 24217);
    weak = server;
    params = server->getParameters();
  }
  REQUIRE_FALSE(weak.expired());

  // still safe to use
  params->setMessageTimeout(kMessageTimeout);
  REQUIRE(params->getMessageTimeout() == kMessageTimeout);

  params.reset();
  REQUIRE(weak.expired());
}

// ---------------------------------------------------------------------------
// Range validation (SECURITY.md M5)
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: window sizes are range checked",
          "[protocol-parameters][validation]") {
  auto server = Server::create("127.0.0.1", 24218);
  auto params = server->getParameters();
  const auto raw = params->getRaw();
  const int k = raw->k;
  const int w = raw->w;
  REQUIRE(k >= w);

  SECTION("k = 0 would make lib60870 malloc(0) and divide by zero") {
    REQUIRE_THROWS_AS(params->setSendWindowSize(0), std::invalid_argument);
    REQUIRE(raw->k == k);
  }
  SECTION("negative k wraps the allocation size") {
    REQUIRE_THROWS_AS(params->setSendWindowSize(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setSendWindowSize(INT32_MIN),
                      std::invalid_argument);
    REQUIRE(raw->k == k);
  }
  SECTION("k above the IEC 60870-5-104 maximum") {
    REQUIRE_THROWS_AS(params->setSendWindowSize(ProtocolParameters::K_MAX + 1),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(params->setSendWindowSize(INT32_MAX),
                      std::invalid_argument);
    REQUIRE(raw->k == k);
  }
  SECTION("k bounds are inclusive") {
    params->setReceiveWindowSize(ProtocolParameters::W_MIN);
    params->setSendWindowSize(ProtocolParameters::K_MIN);
    REQUIRE(raw->k == ProtocolParameters::K_MIN);
    params->setSendWindowSize(ProtocolParameters::K_MAX);
    REQUIRE(raw->k == ProtocolParameters::K_MAX);
  }
  SECTION("w = 0 / negative / too large") {
    REQUIRE_THROWS_AS(params->setReceiveWindowSize(0), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setReceiveWindowSize(-5), std::invalid_argument);
    REQUIRE_THROWS_AS(
        params->setReceiveWindowSize(ProtocolParameters::W_MAX + 1),
        std::invalid_argument);
    REQUIRE(raw->w == w);
  }
  SECTION("w must not exceed k") {
    REQUIRE_THROWS_AS(params->setReceiveWindowSize(k + 1),
                      std::invalid_argument);
    REQUIRE(raw->w == w);
    params->setReceiveWindowSize(k);
    REQUIRE(raw->w == k);
  }
  SECTION("k must not drop below w") {
    REQUIRE_THROWS_AS(params->setSendWindowSize(w - 1), std::invalid_argument);
    REQUIRE(raw->k == k);
    params->setSendWindowSize(w);
    REQUIRE(raw->k == w);
  }
}

TEST_CASE("ProtocolParameters: timeouts are range checked",
          "[protocol-parameters][validation]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 24219);
  auto params = connection->getParameters();
  const auto raw = params->getRaw();
  const sCS104_APCIParameters before = *raw;

  SECTION("t0..t3 must be at least one second") {
    REQUIRE_THROWS_AS(params->setConnectionTimeout(0), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setMessageTimeout(0), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setConfirmInterval(0), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setKeepAliveInterval(0), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setConnectionTimeout(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setMessageTimeout(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setConfirmInterval(-1), std::invalid_argument);
    REQUIRE_THROWS_AS(params->setKeepAliveInterval(-1), std::invalid_argument);
  }
  SECTION("t0..t2 are capped at 255 s, t3 at 48 h") {
    REQUIRE_THROWS_AS(
        params->setConnectionTimeout(ProtocolParameters::T_MAX + 1),
        std::invalid_argument);
    REQUIRE_THROWS_AS(params->setMessageTimeout(ProtocolParameters::T_MAX + 1),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(params->setConfirmInterval(ProtocolParameters::T_MAX + 1),
                      std::invalid_argument);
    REQUIRE_THROWS_AS(
        params->setKeepAliveInterval(ProtocolParameters::T3_MAX + 1),
        std::invalid_argument);
    // lib60870 multiplies by 1000 in int arithmetic
    REQUIRE_THROWS_AS(params->setKeepAliveInterval(INT32_MAX),
                      std::invalid_argument);
  }
  SECTION("bounds are inclusive") {
    params->setConnectionTimeout(ProtocolParameters::T_MIN);
    params->setMessageTimeout(ProtocolParameters::T_MAX);
    params->setConfirmInterval(ProtocolParameters::T_MAX);
    params->setKeepAliveInterval(ProtocolParameters::T3_MAX);
    REQUIRE(raw->t0 == ProtocolParameters::T_MIN);
    REQUIRE(raw->t1 == ProtocolParameters::T_MAX);
    REQUIRE(raw->t2 == ProtocolParameters::T_MAX);
    REQUIRE(raw->t3 == ProtocolParameters::T3_MAX);
    return;
  }

  // rejected writes must leave the block untouched
  REQUIRE(raw->t0 == before.t0);
  REQUIRE(raw->t1 == before.t1);
  REQUIRE(raw->t2 == before.t2);
  REQUIRE(raw->t3 == before.t3);
}

// ---------------------------------------------------------------------------
// Writes while the owner is running
// ---------------------------------------------------------------------------

TEST_CASE("ProtocolParameters: writes are rejected while the server runs",
          "[protocol-parameters][running]") {
  auto server = Server::create("127.0.0.1", 24220);
  auto params = server->getParameters();
  const auto raw = params->getRaw();
  const sCS104_APCIParameters before = *raw;

  server->start();
  REQUIRE(server->isRunning());

  REQUIRE_THROWS_AS(params->setSendWindowSize(kSendWindow), std::runtime_error);
  REQUIRE_THROWS_AS(params->setReceiveWindowSize(kReceiveWindow),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setConnectionTimeout(kConnectionTimeout),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setMessageTimeout(kMessageTimeout),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setConfirmInterval(kConfirmInterval),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setKeepAliveInterval(kKeepAliveInterval),
                    std::runtime_error);

  // reads keep working and nothing changed
  REQUIRE(params->getSendWindowSize() == before.k);
  REQUIRE(raw->w == before.w);
  REQUIRE(raw->t0 == before.t0);
  REQUIRE(raw->t1 == before.t1);
  REQUIRE(raw->t2 == before.t2);
  REQUIRE(raw->t3 == before.t3);

  // the view obtained while running is also locked
  REQUIRE_THROWS_AS(server->getParameters()->setMessageTimeout(kMessageTimeout),
                    std::runtime_error);

  server->stop();
  REQUIRE_FALSE(server->isRunning());

  // writable again after stop
  setAllFields(*params);
  requireAllFields(*raw);
}

TEST_CASE("ProtocolParameters: writes are rejected unless connection is CLOSED",
          "[protocol-parameters][running]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 24221);
  auto params = connection->getParameters();
  const auto raw = params->getRaw();
  const sCS104_APCIParameters before = *raw;

  // never connected -> CLOSED -> writable
  REQUIRE(connection->getState() == CLOSED);
  params->setMessageTimeout(kMessageTimeout);
  REQUIRE(raw->t1 == kMessageTimeout);
  // a closed port is not always refused immediately (e.g. dropped by a
  // firewall), keep the connect attempt short so the test stays fast
  params->setConnectionTimeout(1);
  REQUIRE(raw->t0 == 1);

  // connect() moves the state machine to CLOSED_AWAIT_OPEN (nobody listens on
  // the port, so it never opens) - lib60870 may now read the block
  connection->connect();
  REQUIRE(connection->getState() != CLOSED);

  REQUIRE_THROWS_AS(params->setSendWindowSize(kSendWindow), std::runtime_error);
  REQUIRE_THROWS_AS(params->setReceiveWindowSize(kReceiveWindow),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setConnectionTimeout(kConnectionTimeout),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setMessageTimeout(before.t1), std::runtime_error);
  REQUIRE_THROWS_AS(params->setConfirmInterval(kConfirmInterval),
                    std::runtime_error);
  REQUIRE_THROWS_AS(params->setKeepAliveInterval(kKeepAliveInterval),
                    std::runtime_error);
  REQUIRE(raw->k == before.k);
  REQUIRE(raw->t1 == kMessageTimeout);

  // disconnect() from CLOSED_AWAIT_OPEN defers the close; wait for CLOSED
  connection->disconnect();
  {
    py::gil_scoped_release release;
    for (int i = 0; i < 200 && connection->getState() != CLOSED; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(25));
    }
  }
  REQUIRE(connection->getState() == CLOSED);

  setAllFields(*params);
  requireAllFields(*raw);
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

  setAllAttrs(pp);
  requireAllAttrs(pp);

  // a fresh property access sees the same values
  requireAllAttrs(server.attr("protocol_parameters"));

  server.attr("stop")();
}

TEST_CASE("ProtocolParameters: embedded range errors raise ValueError",
          "[protocol-parameters][validation]") {
  auto c104 = py::module_::import("c104");
  auto Server = c104.attr("Server");

  py::object server = Server("127.0.0.1", 24222, py::arg("tick_rate_ms") = 100);
  py::object pp = server.attr("protocol_parameters");
  const int k = pp.attr("send_window_size").cast<int>();

  auto expectValueError = [&](const char *name, int value) {
    INFO("setting " << name << " = " << value);
    bool raised = false;
    try {
      pp.attr(name) = value;
    } catch (py::error_already_set &e) {
      raised = e.matches(PyExc_ValueError);
      if (!raised) {
        FAIL("unexpected exception: " << e.what());
      }
    }
    REQUIRE(raised);
  };

  expectValueError("send_window_size", 0);
  expectValueError("send_window_size", -1);
  expectValueError("send_window_size", 32768);
  expectValueError("receive_window_size", 0);
  expectValueError("receive_window_size", k + 1);
  expectValueError("connection_timeout", 0);
  expectValueError("message_timeout", 256);
  expectValueError("confirm_interval", -3);
  expectValueError("keep_alive_interval", 172801);

  // untouched
  REQUIRE(pp.attr("send_window_size").cast<int>() == k);

  server.attr("stop")();
}

TEST_CASE("ProtocolParameters: embedded writes while running raise "
          "RuntimeError",
          "[protocol-parameters][running]") {
  auto c104 = py::module_::import("c104");
  auto Server = c104.attr("Server");

  py::object server = Server("127.0.0.1", 24223, py::arg("tick_rate_ms") = 100);
  py::object pp = server.attr("protocol_parameters");
  const int t1 = pp.attr("message_timeout").cast<int>();

  server.attr("start")();
  REQUIRE(server.attr("is_running").cast<bool>());

  bool raised = false;
  try {
    pp.attr("message_timeout") = kMessageTimeout;
  } catch (py::error_already_set &e) {
    raised = e.matches(PyExc_RuntimeError);
    if (!raised) {
      FAIL("unexpected exception: " << e.what());
    }
  }
  REQUIRE(raised);
  REQUIRE(pp.attr("message_timeout").cast<int>() == t1);

  server.attr("stop")();
  pp.attr("message_timeout") = kMessageTimeout;
  REQUIRE(pp.attr("message_timeout").cast<int>() == kMessageTimeout);
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

  setAllAttrs(pp);
  requireAllAttrs(pp);

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

  setAllAttrs(pp);

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
  for (const char *name :
       {"send_window_size", "receive_window_size", "connection_timeout",
        "message_timeout", "confirm_interval", "keep_alive_interval"}) {
    INFO("name '" << name << "' not found in __str__: " << str_value);
    REQUIRE(str_value.find(name) != std::string::npos);
    INFO("name '" << name << "' not found in __repr__: " << repr_value);
    REQUIRE(repr_value.find(name) != std::string::npos);
  }

  // repr additionally carries the address of the parameter block
  REQUIRE(repr_value.find(" at 0x") != std::string::npos);

  server.attr("stop")();
}
