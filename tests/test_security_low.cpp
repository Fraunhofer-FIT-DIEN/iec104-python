/**
 * Copyright 2020-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file test_security_low.cpp
 * @brief regression tests for the Low findings of SECURITY.md (L1-L13)
 *
 * @package iec104-python
 * @namespace tests
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include <chrono>
#include <climits>
#include <cmath>
#include <limits>
#include <memory>

#include "Client.h"
#include "Server.h"
#include "module/Callback.h"
#include "module/TaskLimiter.h"
#include "numbers.h"
#include "object/DataPoint.h"
#include "object/Information.h"
#include "object/Station.h"
#include "remote/Helper.h"
#include "remote/TransportSecurity.h"
#include "remote/message/Batch.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

namespace py = pybind11;
using namespace std::chrono_literals;

namespace {

sCS101_AppLayerParameters lowAppLayerParameters() {
  return {.sizeOfTypeId = 1,
          .sizeOfVSQ = 1,
          .sizeOfCOT = 2,
          .originatorAddress = 0,
          .sizeOfCA = 2,
          .sizeOfIOA = 3,
          .maxSizeOfASDU = 249};
}

/// ASDU with three single points at IOA 1, 2, 3 (non-sequence)
CS101_ASDU makeThreePointAsdu(CS101_AppLayerParameters params) {
  CS101_ASDU asdu = CS101_ASDU_create(params, false, CS101_COT_SPONTANEOUS, 0,
                                      10, false, false);
  CS101_ASDU_setTypeID(asdu, M_SP_NA_1);
  for (int ioa = 1; ioa <= 3; ++ioa) {
    auto io = (InformationObject)SinglePointInformation_create(nullptr, ioa,
                                                               ioa == 2, 0);
    CS101_ASDU_addInformationObject(asdu, io);
    InformationObject_destroy(io);
  }
  return asdu;
}

std::chrono::system_clock::time_point fromMs(const long long ms) {
  return std::chrono::system_clock::time_point(std::chrono::milliseconds(ms));
}

} // namespace

// --------------------------------------------------------------------- L2
TEST_CASE("L2: Batch keeps 24-bit IOAs apart", "[security][low][batch]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto low = station->addPoint(1, M_ME_NC_1);
  auto high = station->addPoint(65537, M_ME_NC_1); // 65537 & 0xFFFF == 1
  auto next = station->addPoint(2, M_ME_NC_1);

  auto batch = Remote::Message::Batch::create(CS101_COT_SPONTANEOUS);
  REQUIRE_NOTHROW(batch->addPoint(low));
  REQUIRE_NOTHROW(batch->addPoint(high));
  REQUIRE(batch->getNumberOfObjects() == 2);
  REQUIRE_NOTHROW(batch->addPoint(next));
  // 1, 2, 65537 are not contiguous
  REQUIRE_FALSE(batch->isSequence());
}

// --------------------------------------------------------------------- L3
TEST_CASE("L3: IncomingMessage does not depend on the owner's parameters",
          "[security][low][remote::message::incoming]") {
  auto params =
      std::make_unique<sCS101_AppLayerParameters>(lowAppLayerParameters());
  CS101_ASDU asdu = makeThreePointAsdu(params.get());
  auto message = Remote::Message::IncomingMessage::create(asdu, params.get());
  CS101_ASDU_destroy(asdu);

  const std::string rawBefore = message->getRawMessageString();

  // the owner changes and then frees its parameter block
  params->sizeOfCA = 1;
  params->sizeOfIOA = 1;
  params->sizeOfCOT = 1;
  params.reset();

  REQUIRE(message->getRawMessageString() == rawBefore);
  message->first();
  REQUIRE(message->getIOA() == 1);
  REQUIRE(message->next());
  REQUIRE(message->next());
  REQUIRE(message->getIOA() == 2);
  REQUIRE(message->next());
  REQUIRE(message->getIOA() == 3);
  REQUIRE_FALSE(message->next());
}

TEST_CASE("L3: cloneAsdu binds the copy to the given parameters",
          "[security][low][remote::helper]") {
  static sCS101_AppLayerParameters a = lowAppLayerParameters();
  static sCS101_AppLayerParameters b = lowAppLayerParameters();
  CS101_ASDU asdu = makeThreePointAsdu(&a);
  CS101_ASDU copy = Remote::cloneAsdu(asdu, &b);
  REQUIRE(copy != nullptr);
  REQUIRE(CS101_ASDU_getTypeID(copy) == M_SP_NA_1);
  REQUIRE(CS101_ASDU_getCA(copy) == 10);
  REQUIRE(CS101_ASDU_getNumberOfElements(copy) == 3);
  REQUIRE(CS101_ASDU_getPayloadSize(copy) == CS101_ASDU_getPayloadSize(asdu));
  CS101_ASDU_destroy(asdu);
  // the source parameters may change now without affecting the copy
  a.sizeOfIOA = 1;
  auto io = CS101_ASDU_getElement(copy, 2);
  REQUIRE(io != nullptr);
  REQUIRE(InformationObject_getObjectAddress(io) == 3);
  InformationObject_destroy(io);
  CS101_ASDU_destroy(copy);

  REQUIRE(Remote::cloneAsdu(nullptr, &b) == nullptr);
}

// --------------------------------------------------------------------- L4
TEST_CASE("L4: NULL cipher suites are rejected",
          "[security][low][remote::TransportSecurity]") {
  auto ts = Remote::TransportSecurity::create();
  REQUIRE_THROWS_AS(ts->setCipherSuites({TLSCipherSuite::RSA_WITH_NULL_SHA256}),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(
      ts->setCipherSuites({TLSCipherSuite::ECDHE_RSA_WITH_AES_128_GCM_SHA256,
                           TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA384}),
      std::invalid_argument);
  REQUIRE_NOTHROW(
      ts->setCipherSuites({TLSCipherSuite::ECDHE_RSA_WITH_AES_128_GCM_SHA256}));
  REQUIRE(Remote::TransportSecurity::isNullCipherSuite(
      TLSCipherSuite::RSA_WITH_NULL_MD5));
  REQUIRE_FALSE(Remote::TransportSecurity::isNullCipherSuite(
      TLSCipherSuite::TLS1_3_AES_128_GCM_SHA256));
}

TEST_CASE("L4: a Client freezes its TransportSecurity on creation",
          "[security][low][remote::TransportSecurity]") {
  auto ts = Remote::TransportSecurity::create();
  REQUIRE_NOTHROW(ts->setVersion(TLS_VERSION_TLS_1_2, TLS_VERSION_TLS_1_3));
  auto client = Client::create(100, 10000, ts);
  // readonly before any add_connection(), as documented
  REQUIRE_THROWS_AS(ts->setVersion(TLS_VERSION_TLS_1_2), std::invalid_argument);
  REQUIRE_THROWS_AS(ts->setHostnameVerification("example.com"),
                    std::invalid_argument);
}

// --------------------------------------------------------------------- L5
TEST_CASE("L5: NormalizedFloat rejects NaN", "[security][low][numbers]") {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  REQUIRE_THROWS_AS(NormalizedFloat(nan), std::overflow_error);
  NormalizedFloat value(0.5f);
  REQUIRE_THROWS_AS(value.set(nan), std::overflow_error);
  REQUIRE_THROWS_AS(value += nan, std::overflow_error);
  NormalizedFloat other(0.25f);
  REQUIRE_THROWS_AS(other *= std::numeric_limits<float>::infinity(),
                    std::overflow_error);
  REQUIRE(value.get() == 0.5f);
}

TEST_CASE("L5: LimitedInteger arithmetic does not overflow int",
          "[security][low][numbers]") {
  LimitedInt16 value(100);
  REQUIRE_THROWS_AS(value += INT_MAX, std::overflow_error);
  REQUIRE_THROWS_AS(value -= INT_MIN, std::overflow_error);
  REQUIRE_THROWS_AS(value *= INT_MAX, std::overflow_error);
  REQUIRE(value.get() == 100);
  // non-assigning operators report the overflow instead of wrapping
  REQUIRE_THROWS_AS(value + INT_MAX, std::overflow_error);
  REQUIRE_THROWS_AS(value * INT_MAX, std::overflow_error);
  REQUIRE((value + 1) == 101);
  value += 1;
  REQUIRE(value.get() == 101);
}

TEST_CASE("L5: timestamps outside the CP56Time2a range are rejected",
          "[security][low][types]") {
  sCP56Time2a time{};
  // pre-1970 (negative), pre-2000 (two digit year ambiguity) and >= 2100
  REQUIRE_THROWS_AS(from_time_point(&time, fromMs(-1000)),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(from_time_point(&time, fromMs(946684799999LL)),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(from_time_point(&time, fromMs(4102444800000LL)),
                    std::invalid_argument);
  // the borders themselves are fine and round-trip
  REQUIRE_NOTHROW(from_time_point(&time, fromMs(946684800000LL)));
  REQUIRE(to_time_point(&time) == fromMs(946684800000LL));
  REQUIRE_NOTHROW(from_time_point(&time, fromMs(4102444799999LL)));
  REQUIRE(to_time_point(&time) == fromMs(4102444799999LL));

  // rejected when the information is created or modified
  REQUIRE_THROWS_AS(Object::SingleInfo::create(true, Quality::None, fromMs(-1)),
                    std::invalid_argument);
  auto info = Object::SingleInfo::create(true);
  REQUIRE_THROWS_AS(info->setRecordedAt(fromMs(4102444800000LL)),
                    std::invalid_argument);
  REQUIRE_FALSE(info->getRecordedAt().has_value());
}

// --------------------------------------------------------------------- L7
TEST_CASE("L7: TaskLimiter bounds pending notification tasks",
          "[security][low][module]") {
  Module::TaskLimiter limiter("c104.Test", 2);
  REQUIRE(limiter.limit() == 2);
  auto a = limiter.acquire("on_test");
  auto b = limiter.acquire("on_test");
  REQUIRE(a);
  REQUIRE(b);
  REQUIRE(limiter.pending() == 2);
  REQUIRE_FALSE(limiter.acquire("on_test"));
  REQUIRE_FALSE(limiter.acquire("on_test"));
  REQUIRE(limiter.dropped() == 2);

  // copies (as held by std::function) keep the single slot
  auto copy = a;
  a.reset();
  REQUIRE(limiter.pending() == 2);
  copy.reset();
  REQUIRE(limiter.pending() == 1);
  REQUIRE(limiter.acquire("on_test"));
  // the temporary was released right away
  REQUIRE(limiter.pending() == 1);
}

TEST_CASE("L7: tokens may outlive their limiter", "[security][low][module]") {
  Module::TaskLimiter::Token token;
  {
    Module::TaskLimiter limiter("c104.Test", 1);
    token = limiter.acquire("on_test");
    REQUIRE(token);
  }
  REQUIRE_NOTHROW(token.reset());
}

TEST_CASE("L7: Server and Client own a notification limiter",
          "[security][low][module]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  REQUIRE(server->getNotificationLimiter().limit() ==
          Module::MAX_PENDING_NOTIFICATION_TASKS);
  auto client = Client::create();
  REQUIRE(client->getNotificationLimiter().limit() ==
          Module::MAX_PENDING_NOTIFICATION_TASKS);
  auto token = client->acquireNotificationToken("on_test");
  REQUIRE(token);
  REQUIRE(client->getNotificationLimiter().pending() == 1);
}

// --------------------------------------------------------------------- L8
TEST_CASE("L8: duplicate stations and connections are refused",
          "[security][low][connection]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 2404);
  REQUIRE(connection != nullptr);
  REQUIRE(client->addConnection("127.0.0.1", 2404) == nullptr);
  REQUIRE(client->getConnections().size() == 1);

  REQUIRE(connection->addStation(5) != nullptr);
  REQUIRE(connection->addStation(5) == nullptr);
  REQUIRE(connection->getStations().size() == 1);
}

// --------------------------------------------------------------------- L9
TEST_CASE("L9: start/stop of the run loop never loses the wake-up",
          "[security][low][client]") {
  auto client = Client::create();
  py::gil_scoped_release const release;
  // a lost notify_all() in stopRunThread() used to block the join forever
  for (int i = 0; i < 200; ++i) {
    client->start();
    client->stop();
  }
  SUCCEED();
}

// --------------------------------------------------------------------- L10
TEST_CASE("L10: message snapshots have an independent cursor",
          "[security][low][remote::message::incoming]") {
  static sCS101_AppLayerParameters params = lowAppLayerParameters();
  CS101_ASDU asdu = makeThreePointAsdu(&params);
  auto message = Remote::Message::IncomingMessage::create(asdu, &params);
  CS101_ASDU_destroy(asdu);

  // fresh message: the snapshot starts fresh as well
  auto fresh = message->snapshot();
  REQUIRE(fresh->next());
  REQUIRE(fresh->getIOA() == 1);

  REQUIRE(message->next());
  REQUIRE(message->next());
  REQUIRE(message->getIOA() == 2);

  auto snapshot = message->snapshot();
  REQUIRE(snapshot.get() != message.get());
  REQUIRE(snapshot->getIOA() == 2);
  REQUIRE(snapshot->getInfo() != nullptr);
  REQUIRE(std::get<bool>(snapshot->getInfo()->getValue()) == true);

  // moving the snapshot's cursor does not touch the original
  snapshot->first();
  REQUIRE(snapshot->getIOA() == 1);
  REQUIRE(message->getIOA() == 2);
  REQUIRE(message->next());
  REQUIRE(message->getIOA() == 3);
  REQUIRE_FALSE(message->next());
}

// --------------------------------------------------------------------- L11
namespace {
/// a type without pybind11 binding, converting it to python throws
struct NotBound {};
} // namespace

TEST_CASE("L11: an unconvertible callback argument does not terminate",
          "[security][low][callback]") {
  py::exec(R"(
def _l11_cb(value) -> None:
    pass
)");
  py::object callable = py::module_::import("__main__").attr("_l11_cb");
  Module::Callback<void> cb("Test.l11", "(value) -> None");
  cb.reset(callable);
  REQUIRE(cb.is_set());
  // used to escape the noexcept call() as py::cast_error -> std::terminate
  REQUIRE_FALSE(cb.call(NotBound{}));
  REQUIRE(cb.is_set());
  // the caught cast_error left a pending TypeError indicator behind, which
  // used to be attributed to the next python call on this thread
  REQUIRE(PyErr_Occurred() == nullptr);

  Module::Callback<bool> cbr("Test.l11r", "(value) -> None");
  cbr.reset(callable);
  REQUIRE_FALSE(cbr.call(NotBound{}).has_value());
  REQUIRE(PyErr_Occurred() == nullptr);
}

// --------------------------------------------------------------------- L12
TEST_CASE("L12: DoubleCmd without timestamp has no recorded_at",
          "[security][low][object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto point = station->addPoint(20, C_DC_NA_1);
  REQUIRE(std::dynamic_pointer_cast<Object::DoubleCmd>(point->getInfo()));
  REQUIRE_FALSE(point->getRecordedAt().has_value());
  auto timed = station->addPoint(21, C_DC_TA_1);
  REQUIRE(timed->getRecordedAt().has_value());
}

TEST_CASE("L12: Point.info stores a private copy",
          "[security][low][object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto plain = station->addPoint(30, M_SP_NA_1);
  auto timed = station->addPoint(31, M_SP_TB_1);

  const auto when = fromMs(1711111111111LL);
  auto info = Object::SingleInfo::create(true, Quality::None, when);
  plain->setInfo(info);
  timed->setInfo(info);

  // the caller's object is untouched by the timestamp normalisation
  REQUIRE(info->getRecordedAt() == when);
  REQUIRE_FALSE(plain->getRecordedAt().has_value());
  REQUIRE(timed->getRecordedAt() == when);

  // and the points do not share state with it or with each other
  REQUIRE(plain->getInfo().get() != info.get());
  REQUIRE(timed->getInfo().get() != info.get());
  REQUIRE(plain->getInfo().get() != timed->getInfo().get());
  info->setValue(false);
  REQUIRE(std::get<bool>(plain->getValue()) == true);
  REQUIRE(std::get<bool>(timed->getValue()) == true);

  // the wrong type is still rejected
  REQUIRE_THROWS_AS(
      plain->setInfo(Object::DoubleInfo::create(IEC60870_DOUBLE_POINT_ON)),
      std::invalid_argument);
  REQUIRE_THROWS_AS(plain->setInfo(nullptr), std::invalid_argument);
}

TEST_CASE("L12: Server validates the bind address and port",
          "[security][low][server]") {
  REQUIRE_THROWS_AS(Server::create("not-an-ip", 2404, 100),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(Server::create(std::string("127.0.0.1\0x", 11), 2404, 100),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(Server::create("127.0.0.1", 0, 100), std::invalid_argument);
  REQUIRE_NOTHROW(Server::create("0.0.0.0", 65535, 100));
}

TEST_CASE("L12: max_connections can be changed without touching lib60870",
          "[security][low][server]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  server->setMaxOpenConnections(3);
  REQUIRE(server->getMaxOpenConnections() == 3);
  server->setMaxOpenConnections(0);
  REQUIRE(server->getMaxOpenConnections() == 0);
}

TEST_CASE("L12: raw frames are not truncated to 255 bytes",
          "[security][low][server]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  // no callback registered: must simply return, also for odd sizes
  unsigned char buf[300] = {0};
  REQUIRE_NOTHROW(server->onReceiveRaw(buf, 300));
  REQUIRE_NOTHROW(server->onSendRaw(buf, 0));
  REQUIRE_NOTHROW(server->onSendRaw(nullptr, 5));
}
