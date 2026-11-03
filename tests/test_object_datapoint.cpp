/**
 * Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology
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
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <atomic>
#include <thread>
#include <vector>

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

TEST_CASE("Create point", "[object::point]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(point->getStation().get() == station.get());
  REQUIRE(point->getInformationObjectAddress() == 11);
  REQUIRE(point->getRelatedInformationObjectAddress() == std::nullopt);
  REQUIRE(point->getRelatedInformationObjectAutoReturn() == false);
  REQUIRE(point->getType() == IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(point->getReportInterval_ms() == 0);
  REQUIRE(std::get<Quality>(point->getInfo()->getQuality()) == Quality::None);
  REQUIRE(std::get<Quality>(point->getQuality()) == Quality::None);
  REQUIRE(std::get<bool>(point->getInfo()->getValue()) == false);
  REQUIRE(std::get<bool>(point->getValue()) == false);
  REQUIRE(point->getProcessedAt() >
          std::chrono::system_clock::time_point::min());
  REQUIRE(point->getRecordedAt().has_value() == false);
}

TEST_CASE("Set point value", "[object::point]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_ME_TE_1);

  point->setInfo(Object::ScaledInfo::create(
      LimitedInt16(334), Quality::Invalid,
      std::chrono::system_clock::time_point(
          std::chrono::milliseconds(1711111111111))));
  REQUIRE(std::get<LimitedInt16>(point->getValue()).get() ==
          LimitedInt16(334).get());
  REQUIRE(point->getRecordedAt().value() ==
          std::chrono::system_clock::time_point(
              std::chrono::milliseconds(1711111111111)));
  REQUIRE(std::get<Quality>(point->getQuality()) == Quality::Invalid);
}

TEST_CASE("Set point value via message", "[object::point]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_DC_TA_1);

  sCS101_AppLayerParameters appLayerParameters{.sizeOfTypeId = 1,
                                               .sizeOfVSQ = 0,
                                               .sizeOfCOT = 2,
                                               .originatorAddress = 99,
                                               .sizeOfCA = 2,
                                               .sizeOfIOA = 3,
                                               .maxSizeOfASDU = 249};
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CS101_ASDU asdu = CS101_ASDU_create(
      &appLayerParameters, false, CS101_COT_ACTIVATION, 0, 14, false, false);
  CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
  InformationObject io = (InformationObject)DoubleCommandWithCP56Time2a_create(
      nullptr, 14, 1, false, 0, &time);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message =
      Remote::Message::IncomingMessage::create(asdu, &appLayerParameters);

  point->onReceive(message);
  REQUIRE(std::get<DoublePointValue>(point->getValue()) ==
          IEC60870_DOUBLE_POINT_OFF);
  REQUIRE(point->getRecordedAt() ==
          std::chrono::system_clock::time_point(
              std::chrono::milliseconds(1680517666000)));
  REQUIRE(std::get<std::monostate>(point->getQuality()) == std::monostate{});

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Concurrent point info access", "[object::point]") {
  // Regression: onReceive moved info out before replacing it, so a concurrent
  // reader (e.g. Python reading point.value) could dereference a null info.
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_SP_NA_1);

  sCS101_AppLayerParameters appLayerParameters{.sizeOfTypeId = 1,
                                               .sizeOfVSQ = 1,
                                               .sizeOfCOT = 2,
                                               .originatorAddress = 0,
                                               .sizeOfCA = 2,
                                               .sizeOfIOA = 3,
                                               .maxSizeOfASDU = 249};
  std::vector<std::shared_ptr<Remote::Message::IncomingMessage>> messages;
  for (bool const value : {true, false}) {
    CS101_ASDU asdu = CS101_ASDU_create(
        &appLayerParameters, false, CS101_COT_SPONTANEOUS, 0, 10, false, false);
    InformationObject io = (InformationObject)SinglePointInformation_create(
        nullptr, 11, value, IEC60870_QUALITY_GOOD);
    CS101_ASDU_addInformationObject(asdu, io);
    auto message =
        Remote::Message::IncomingMessage::create(asdu, &appLayerParameters);
    REQUIRE(message->next());
    messages.push_back(message);
    InformationObject_destroy(io);
    CS101_ASDU_destroy(asdu);
  }

  constexpr int iterations = 200000;
  std::atomic_bool done{false};
  std::atomic_int reads{0};

  std::thread receiver([&]() {
    for (int i = 0; i < iterations; i++) {
      point->onReceive(messages[i % 2]);
    }
    done = true;
  });
  std::thread replacer([&]() {
    while (!done) {
      point->setInfo(Object::SingleInfo::create(true));
    }
  });
  std::vector<std::thread> readers;
  for (int r = 0; r < 4; r++) {
    readers.emplace_back([&]() {
      while (!done) {
        (void)point->getValue();
        (void)point->getQuality();
        (void)point->getRecordedAt();
        (void)point->getProcessedAt();
        (void)point->toString();
        reads++;
      }
    });
  }

  receiver.join();
  replacer.join();
  for (auto &reader : readers) {
    reader.join();
  }

  REQUIRE(reads > 0);
  REQUIRE(point->getInfo() != nullptr);
}

// ---------------------------------------------------------------------------
// on_receive fail-closed semantics (regression for SECURITY H2)
// ---------------------------------------------------------------------------

namespace {

void ensureReceiveCallbacks() {
  static bool inited = false;
  if (inited)
    return;
  inited = true;
  py::exec(R"(
import c104

_rx_seen_new_value = None
_rx_seen_prev_value = None

def _rx_record(point: c104.Point, previous_info: c104.Information) -> None:
    global _rx_seen_new_value, _rx_seen_prev_value
    _rx_seen_new_value = point.value
    _rx_seen_prev_value = previous_info.value

def _rx_success(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    _rx_record(point, previous_info)
    return c104.ResponseState.SUCCESS

def _rx_failure(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    _rx_record(point, previous_info)
    return c104.ResponseState.FAILURE

def _rx_none(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    _rx_record(point, previous_info)
    return c104.ResponseState.NONE

def _rx_raising(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    _rx_record(point, previous_info)
    raise RuntimeError("validator crashed")

def _rx_no_return(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    _rx_record(point, previous_info)
    if point.value == 99:
        return c104.ResponseState.SUCCESS
)");
}

py::object rxCallable(const char *name) {
  return py::module_::import("__main__").attr(name);
}

py::object rxGlobal(const char *name) {
  return py::module_::import("__main__").attr(name);
}

// single command (C_SC_NA_1) carrying @p value for IOA 11 at CA 10
std::shared_ptr<Remote::Message::IncomingMessage>
makeSingleCommand(sCS101_AppLayerParameters &params, bool value) {
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_ACTIVATION, 0,
                                      10, false, false);
  InformationObject io =
      (InformationObject)SingleCommand_create(nullptr, 11, value, false, 0);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message = Remote::Message::IncomingMessage::create(asdu, &params);
  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
  return message;
}

} // namespace

TEST_CASE("DataPoint onReceive applies the value only on SUCCESS or NONE",
          "[object::point][on_receive][fail-closed]") {
  ensureReceiveCallbacks();
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);
  point->setInfo(Object::SingleCmd::create(false));

  sCS101_AppLayerParameters params{.sizeOfTypeId = 1,
                                   .sizeOfVSQ = 1,
                                   .sizeOfCOT = 2,
                                   .originatorAddress = 0,
                                   .sizeOfCA = 2,
                                   .sizeOfIOA = 3,
                                   .maxSizeOfASDU = 249};
  auto on_message = makeSingleCommand(params, true);
  REQUIRE(on_message->next());

  SECTION("SUCCESS keeps the new value") {
    auto cb = rxCallable("_rx_success");
    point->setOnReceiveCallback(cb);
    REQUIRE(point->onReceive(on_message) == RESPONSE_STATE_SUCCESS);
    REQUIRE(std::get<bool>(point->getValue()) == true);
  }

  SECTION("NONE keeps the new value (handler responds manually)") {
    auto cb = rxCallable("_rx_none");
    point->setOnReceiveCallback(cb);
    REQUIRE(point->onReceive(on_message) == RESPONSE_STATE_NONE);
    REQUIRE(std::get<bool>(point->getValue()) == true);
  }

  SECTION("FAILURE restores the previous value") {
    auto cb = rxCallable("_rx_failure");
    point->setOnReceiveCallback(cb);
    REQUIRE(point->onReceive(on_message) == RESPONSE_STATE_FAILURE);
    REQUIRE(std::get<bool>(point->getValue()) == false);
  }

  SECTION("raising handler fails closed, restores value, stays registered") {
    auto cb = rxCallable("_rx_raising");
    point->setOnReceiveCallback(cb);
    REQUIRE(point->onReceive(on_message) == RESPONSE_STATE_FAILURE);
    REQUIRE(std::get<bool>(point->getValue()) == false);
    // second attempt is rejected as well: the handler was not removed
    REQUIRE(point->onReceive(on_message) == RESPONSE_STATE_FAILURE);
    REQUIRE(std::get<bool>(point->getValue()) == false);
    REQUIRE(PyErr_Occurred() == nullptr);
  }

  SECTION("missing return statement (None) fails closed") {
    auto cb = rxCallable("_rx_no_return");
    point->setOnReceiveCallback(cb);
    REQUIRE(point->onReceive(on_message) == RESPONSE_STATE_FAILURE);
    REQUIRE(std::get<bool>(point->getValue()) == false);
  }

  // in every section the handler observed the documented view: the point
  // already carried the new value, previous_info the old one
  REQUIRE(py::cast<bool>(rxGlobal("_rx_seen_new_value")) == true);
  REQUIRE(py::cast<bool>(rxGlobal("_rx_seen_prev_value")) == false);
}
