/**
 * Copyright 2024-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file test_point_logic.cpp
 * @brief Tests for DataPoint callback and auto-return logic: on_before_auto_
 *        transmit, related information object auto-return, and report interval
 *        nextReportAt() calculation.
 */

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "Client.h"
#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"

namespace py = pybind11;

using namespace Object;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// Python callback - defined in the embedded interpreter (main.cpp holds a
// single py::scoped_interpreter; we never create a second one). It records a
// side-effect in a global counter so the C++ side can verify the callback was
// actually invoked by calling onBeforeAutoTransmit() manually.
// ---------------------------------------------------------------------------
void ensurePythonCallbacks() {
  try {
    py::exec(R"(
import c104

_cb_auto_transmit_count = 0

def _cb_on_before_auto_transmit(point: c104.Point) -> None:
    global _cb_auto_transmit_count
    _cb_auto_transmit_count += 1
)",
             py::globals());
  } catch (const std::exception &) {
    // interpreter unavailable - callback branches stay uncovered but nothing
    // may crash
  }
}

int autoTransmitCount() {
  try {
    return py::cast<int>(py::globals()["_cb_auto_transmit_count"]);
  } catch (const std::exception &) {
    return -1;
  }
}

} // namespace

// ---------------------------------------------------------------------------
// on_before_auto_transmit callback
// ---------------------------------------------------------------------------

TEST_CASE("DataPoint: on_before_auto_transmit callback executes",
          "[object::point]") {
  ensurePythonCallbacks();
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto point = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  // no callback registered -> no side-effect
  int const before = autoTransmitCount();
  point->onBeforeAutoTransmit();
  REQUIRE(autoTransmitCount() == before);

  // register the callback and call onBeforeAutoTransmit() manually
  py::object cb = py::globals()["_cb_on_before_auto_transmit"];
  point->setOnBeforeAutoTransmitCallback(cb);
  point->onBeforeAutoTransmit();
  REQUIRE(autoTransmitCount() == before + 1);
}

TEST_CASE("DataPoint: on_before_auto_transmit callback throws on client point",
          "[object::point]") {
  ensurePythonCallbacks();
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 2404);
  auto station = connection->addStation(20);

  auto point = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(point->getStation()->isLocal() == false);

  py::object cb = py::globals()["_cb_on_before_auto_transmit"];
  REQUIRE_THROWS_AS(point->setOnBeforeAutoTransmitCallback(cb),
                    std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Related Information Object auto-return
// ---------------------------------------------------------------------------

TEST_CASE("DataPoint: related information object auto-return round-trip",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // control point with a related monitoring point (IOA 99) configured
  auto point = station->addPoint(1, IEC60870_5_TypeID::C_SC_NA_1, 0, 99);

  REQUIRE(point->getRelatedInformationObjectAddress() == 99);
  REQUIRE(point->getRelatedInformationObjectAutoReturn() == false);

  point->setRelatedInformationObjectAutoReturn(true);
  REQUIRE(point->getRelatedInformationObjectAutoReturn() == true);

  point->setRelatedInformationObjectAutoReturn(false);
  REQUIRE(point->getRelatedInformationObjectAutoReturn() == false);
}

TEST_CASE("DataPoint: auto-return rejects monitoring-only point type",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // monitoring point type (not in [C_SC_NA_1, C_BO_TA_1]) with related IOA set
  auto point = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1, 0, 99);
  REQUIRE_THROWS_AS(point->setRelatedInformationObjectAutoReturn(true),
                    std::invalid_argument);
}

TEST_CASE("DataPoint: auto-return rejects missing related IOA",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // control point without any related IOA configured
  auto point = station->addPoint(1, IEC60870_5_TypeID::C_SC_NA_1);
  REQUIRE(point->getRelatedInformationObjectAddress().has_value() == false);
  REQUIRE_THROWS_AS(point->setRelatedInformationObjectAutoReturn(true),
                    std::invalid_argument);
}

TEST_CASE("DataPoint: related IOA reports undefined as nullopt",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // fresh control point without related IOA -> must read back nullopt (not 0)
  auto point = station->addPoint(1, IEC60870_5_TypeID::C_SC_NA_1);
  REQUIRE(point->getRelatedInformationObjectAddress().has_value() == false);

  // set a valid related IOA
  point->setRelatedInformationObjectAddress(123);
  REQUIRE(point->getRelatedInformationObjectAddress().has_value() == true);
  REQUIRE(point->getRelatedInformationObjectAddress().value() == 123);

  // explicitly clear it again -> nullopt, not 0
  point->setRelatedInformationObjectAddress(std::nullopt);
  REQUIRE(point->getRelatedInformationObjectAddress().has_value() == false);
}

TEST_CASE("DataPoint: fresh measurement point has no related IOA",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // a freshly created measurement point must report no related IOA (nullopt),
  // never the sentinel 0 which is a valid IOA
  auto measurement = station->addPoint(11, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(measurement->getRelatedInformationObjectAddress().has_value() ==
          false);

  // a control point created with a related IOA reports it as a value
  auto command = station->addPoint(12, IEC60870_5_TypeID::C_SC_NA_1, 0, 99);
  REQUIRE(command->getRelatedInformationObjectAddress().has_value() == true);
  REQUIRE(command->getRelatedInformationObjectAddress().value() == 99);
}

// ---------------------------------------------------------------------------
// reportInterval / nextReportAt
// ---------------------------------------------------------------------------

TEST_CASE("DataPoint: nextReportAt returns nullopt with no report interval",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto point = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  REQUIRE(point->getReportInterval_ms() == 0);
  REQUIRE(point->nextReportAt().has_value() == false);
}

TEST_CASE("DataPoint: nextReportAt is last-sent plus report_ms",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto point = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1, 500);

  REQUIRE(point->getReportInterval_ms() == 500);
  REQUIRE(point->nextReportAt().has_value() == true);

  // setProcessedAt resets the internal lastSentAt reference to now(). Capture
  // a steady-clock reference immediately before to bound the expected value.
  auto reference = std::chrono::steady_clock::now();
  point->setProcessedAt(std::chrono::system_clock::now());

  auto next = point->nextReportAt().value();
  // nextReportAt() must be within (500ms .. 500ms + slack) after reference.
  auto elapsed = next - reference;
  REQUIRE(elapsed >= 500ms);
  REQUIRE(elapsed < 500ms + 1s);

  // A larger report interval yields a correspondingly later next report time.
  auto previous = next;
  point->setReportInterval_ms(1000);
  point->setProcessedAt(std::chrono::system_clock::now());
  auto nextLarger = point->nextReportAt().value();
  REQUIRE(nextLarger >= previous);
}
