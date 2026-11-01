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
 * @file test_server.cpp
 * @brief Drive src/Server.cpp public methods + static handlers to high line
 * coverage using the mock sIPeerConnection technique (tests/mock_helpers.h)
 * plus a few real Server::start()/stop() lifecycle calls.
 */

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include "Server.h"
#include "mock_helpers.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/message/Batch.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/OutgoingMessage.h"
#include "remote/message/PointCommand.h"
#include "remote/message/PointMessage.h"
#include "types.h"

namespace py = pybind11;
using namespace Remote;

namespace {

// Register several well-typed python callbacks in the embedded interpreter
// (main.cpp holds the process-lifetime scoped_interpreter).
void ensureServerCallbacks() {
  static bool inited = false;
  if (inited)
    return;
  inited = true;
  py::exec(R"(
import c104
from datetime import datetime
def _srv_void(server: c104.Server, data: bytes) -> None:
    pass
def _srv_clock(server: c104.Server, ip: str, date_time: datetime) -> c104.ResponseState:
    return c104.ResponseState.SUCCESS
def _srv_unexpected(server: c104.Server, message: c104.IncomingMessage, cause: c104.Umc) -> None:
    pass
def _srv_connect(server: c104.Server, ip: str) -> bool:
    return True
)");
}

py::object getCallable(const char *name) {
  return py::module_::import("__main__").attr(name);
}

// Make an ASDU containing a single command information object.
CS101_ASDU makeCommandAsdu(sCS101_AppLayerParameters &params,
                           IEC60870_5_TypeID type, uint_fast32_t ioa,
                           uint_fast16_t ca, CS101_CauseOfTransmission cot,
                           uint_fast8_t oa, bool select) {
  CS101_ASDU asdu =
      CS101_ASDU_create(&params, false, cot, oa, ca, false, false);
  CS101_ASDU_setTypeID(asdu, type);
  InformationObject io = nullptr;
  switch (type) {
  case IEC60870_5_TypeID::C_SC_NA_1:
    io = (InformationObject)SingleCommand_create(nullptr, (int)ioa, true,
                                                 select, 0);
    break;
  case IEC60870_5_TypeID::C_DC_NA_1:
    io = (InformationObject)DoubleCommand_create(nullptr, (int)ioa, 1, select,
                                                 0);
    break;
  case IEC60870_5_TypeID::C_RC_NA_1:
    io = (InformationObject)StepCommand_create(nullptr, (int)ioa,
                                               IEC60870_STEP_LOWER, select, 0);
    break;
  case IEC60870_5_TypeID::C_SE_NC_1:
    io = (InformationObject)SetpointCommandShort_create(nullptr, (int)ioa, 0.5f,
                                                        select, 0);
    break;
  case IEC60870_5_TypeID::C_CS_NA_1: {
    sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
    CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
    io = (InformationObject)ClockSynchronizationCommand_create(nullptr,
                                                               (int)ioa, &time);
    break;
  }
  default:
    break;
  }
  CS101_ASDU_addInformationObject(asdu, io);
  return asdu;
}

} // namespace

// ---------------------------------------------------------------------------
// Getters + basic construction / destructor
// ---------------------------------------------------------------------------

TEST_CASE("Server: construction validator and simple getters",
          "[server][getters]") {
  REQUIRE_THROWS_AS(Server::create("127.0.0.1", 2404, 10), std::range_error);
  REQUIRE_THROWS_AS(Server::create("127.0.0.1", 2404, 49), std::range_error);

  auto server = Server::create("127.0.0.1", 2404, 100, 10000, 5);
  REQUIRE(server->getIP() == "127.0.0.1");
  REQUIRE(server->getPort() == 2404);
  REQUIRE(server->getTickRate_ms() == 100);
  REQUIRE(server->getMaxOpenConnections() == 5);
  REQUIRE(server->getParameters() != nullptr);
  REQUIRE(server->toString().find("104.Server") != std::string::npos);
}

TEST_CASE("Server: create with transport security", "[server][tls]") {
  auto ts = Remote::TransportSecurity::create();
  auto server = Server::create("127.0.0.1", 2405, 100, 10000, 0, ts);
  REQUIRE(server->getIP() == "127.0.0.1");
}

TEST_CASE("Server: station add/get/remove/has", "[server][stations]") {
  auto server = Server::create("127.0.0.1", 2404);
  REQUIRE(server->hasStations() == false);
  REQUIRE(server->getStations().empty());

  auto s1 = server->addStation(1);
  REQUIRE(s1 != nullptr);
  REQUIRE(server->hasStations() == true);
  REQUIRE(server->getStation(1).get() == s1.get());
  REQUIRE(server->getStation(2).get() == nullptr);
  REQUIRE(server->hasStation(1) == true);
  REQUIRE(server->hasStation(2) == false);
  REQUIRE(server->hasStation(IEC60870_GLOBAL_COMMON_ADDRESS) == true);

  // duplicate add returns nullptr
  auto dup = server->addStation(1);
  REQUIRE(dup.get() == nullptr);

  // remove existing
  REQUIRE(server->removeStation(1) == true);
  REQUIRE(server->hasStation(1) == false);
  // remove non-existing
  REQUIRE(server->removeStation(99) == false);
}

// ---------------------------------------------------------------------------
// Connection limit + callbacks setters
// ---------------------------------------------------------------------------

TEST_CASE("Server: max open connections setter", "[server][limits]") {
  auto server = Server::create("127.0.0.1", 2404);
  REQUIRE(server->getMaxOpenConnections() == 0);
  server->setMaxOpenConnections(3);
  REQUIRE(server->getMaxOpenConnections() == 3);
  // set same value -> early return (no state change)
  server->setMaxOpenConnections(3);
  REQUIRE(server->getMaxOpenConnections() == 3);
  server->setMaxOpenConnections(0);
  REQUIRE(server->getMaxOpenConnections() == 0);
}

TEST_CASE("Server: callback setters", "[server][callbacks]") {
  ensureServerCallbacks();
  auto server = Server::create("127.0.0.1", 2404);

  // valid signatures
  {
    auto cb = getCallable("_srv_void");
    REQUIRE_NOTHROW(server->setOnReceiveRawCallback(cb));
    REQUIRE_NOTHROW(server->setOnSendRawCallback(cb));
  }
  {
    auto cb = getCallable("_srv_clock");
    REQUIRE_NOTHROW(server->setOnClockSyncCallback(cb));
  }
  {
    auto cb = getCallable("_srv_unexpected");
    REQUIRE_NOTHROW(server->setOnUnexpectedMessageCallback(cb));
  }
  {
    auto cb = getCallable("_srv_connect");
    REQUIRE_NOTHROW(server->setOnConnectCallback(cb));
  }

  // bad signature -> invalid_argument; reset to none OK
  {
    auto cb = getCallable("_srv_connect");
    REQUIRE_THROWS_AS(server->setOnReceiveRawCallback(cb),
                      std::invalid_argument);
    py::object none = py::none();
    REQUIRE_NOTHROW(server->setOnReceiveRawCallback(none));
  }
}

// ---------------------------------------------------------------------------
// Raw message handling (onReceiveRaw / onSendRaw no-callback + callback)
// ---------------------------------------------------------------------------

TEST_CASE("Server: rawMessageHandler without callbacks", "[server][raw]") {
  auto server = Server::create("127.0.0.1", 2404);
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  uint_fast8_t msg[8] = {0x68, 0x04, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00};
  // no callbacks configured -> onReceiveRaw / onSendRaw early-out
  Server::rawMessageHandler(server.get(), mock, msg, 8, false);
  Server::rawMessageHandler(server.get(), mock, msg, 8, true);
  REQUIRE(state.sendASDU_count == 0);
}

TEST_CASE("Server: rawMessageHandler with callbacks", "[server][raw]") {
  ensureServerCallbacks();
  auto server = Server::create("127.0.0.1", 2404);
  auto cb = getCallable("_srv_void");
  server->setOnReceiveRawCallback(cb);
  server->setOnSendRawCallback(cb);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  uint_fast8_t msg[8] = {0x68, 0x04, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00};
  Server::rawMessageHandler(server.get(), mock, msg, 8, false); // recv
  Server::rawMessageHandler(server.get(), mock, msg, 8, true);  // send

  // callbacks are queued as tasks; give the queue a moment (best effort)
  std::this_thread::sleep_for(std::chrono::milliseconds(20));
  REQUIRE(state.sendASDU_count == 0);
}

TEST_CASE("Server: rawMessageHandler during shutdown (null instance)",
          "[server][raw]") {
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  uint_fast8_t msg[4] = {0, 0, 0, 0};
  // No instance -> early return, no crash
  Server::rawMessageHandler(dangling, nullptr, msg, 4, false);
}

// ---------------------------------------------------------------------------
// connection events (the big branchy handler)
// ---------------------------------------------------------------------------

TEST_CASE(
    "Server: connection event handler OPENED/CLOSED/ACTIVATED/DEACTIVATED",
    "[server][events]") {
  auto server = Server::create("127.0.0.1", 2404);
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  REQUIRE(server->isExistingConnection(mock) == false);
  REQUIRE(server->hasActiveConnections() == false);
  REQUIRE(server->getActiveConnectionCount() == 0);

  // OPENED: adds to map as invalid receiver
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_OPENED);
  REQUIRE(server->isExistingConnection(mock) == true);
  REQUIRE(server->getActiveConnectionCount() == 0);

  // OPENED again (already in map, second==false) -> no downcount
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_OPENED);
  REQUIRE(server->getActiveConnectionCount() == 0);

  // ACTIVATED: mark valid -> activeConnections++
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);
  REQUIRE(server->getActiveConnectionCount() == 1);
  REQUIRE(server->hasActiveConnections() == true);

  // ACTIVATED again (already active) -> no double increment
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);
  REQUIRE(server->getActiveConnectionCount() == 1);

  // DEACTIVATED: active -> false, decrement
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_DEACTIVATED);
  REQUIRE(server->getActiveConnectionCount() == 0);

  // DEACTIVATED again (second==false) -> no change
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_DEACTIVATED);
  REQUIRE(server->getActiveConnectionCount() == 0);

  // reopen + activate, then CLOSED removes entry + active--
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_OPENED);
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);
  REQUIRE(server->getActiveConnectionCount() == 1);
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_CLOSED);
  REQUIRE(server->isExistingConnection(mock) == false);
  REQUIRE(server->getActiveConnectionCount() == 0);
}

TEST_CASE("Server: connection event CLOSED on active entry & unknown close",
          "[server][events]") {
  auto server = Server::create("127.0.0.1", 2404);
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  // CLOSED on a connection NOT in map -> no-op, still schedules drop
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_CLOSED);

  // OPENED then CLOSED (not active at close time) -> no downcount, entry
  // removed
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_OPENED);
  Server::connectionEventHandler(server.get(), mock,
                                 CS104_CON_EVENT_CONNECTION_CLOSED);
  REQUIRE(server->isExistingConnection(mock) == false);
}

TEST_CASE("Server: connection event in shutdown (null instance)",
          "[server][events]") {
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  // no instance -> early return, no crash
  Server::connectionEventHandler(dangling, mock,
                                 CS104_CON_EVENT_CONNECTION_OPENED);
  Server::connectionEventHandler(dangling, mock,
                                 CS104_CON_EVENT_CONNECTION_CLOSED);
}

TEST_CASE("Server: connectionRequestHandler null instance and default accept",
          "[server][connect]") {
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  REQUIRE(Server::connectionRequestHandler(dangling, "127.0.0.1") == false);

  auto server = Server::create("127.0.0.1", 2404);
  // no on_connect callback -> returns true
  REQUIRE(Server::connectionRequestHandler(server.get(), "127.0.0.1") == true);
}

TEST_CASE("Server: connectionRequestHandler with python callback",
          "[server][connect]") {
  ensureServerCallbacks();
  auto server = Server::create("127.0.0.1", 2404);
  auto cb = getCallable("_srv_connect");
  server->setOnConnectCallback(cb);
  REQUIRE(Server::connectionRequestHandler(server.get(), "127.0.0.1") == true);
}

// ---------------------------------------------------------------------------
// onUnexpectedMessage (all causes)
// ---------------------------------------------------------------------------

TEST_CASE("Server: onUnexpectedMessage all causes", "[server][unexpected]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_ACTIVATION, 0,
                                      1, false, false);
  InformationObject io = (InformationObject)SinglePointInformation_create(
      nullptr, 100, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message = Remote::Message::IncomingMessage::create(asdu, &params);

  server->onUnexpectedMessage(mock, message, INVALID_TYPE_ID);
  server->onUnexpectedMessage(mock, message, MISMATCHED_TYPE_ID);
  server->onUnexpectedMessage(mock, message, UNKNOWN_TYPE_ID);
  server->onUnexpectedMessage(mock, message, INVALID_COT);
  server->onUnexpectedMessage(mock, message, UNKNOWN_COT);
  server->onUnexpectedMessage(mock, message, UNKNOWN_CA);
  server->onUnexpectedMessage(mock, message, UNKNOWN_IOA);
  server->onUnexpectedMessage(mock, message, UNIMPLEMENTED_GROUP);
  server->onUnexpectedMessage(mock, message, NO_ERROR_CAUSE); // default
  REQUIRE(state.sendASDU_count >= 7); // each non-group cause triggers a send

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: onUnexpectedMessage with python callback",
          "[server][unexpected]") {
  ensureServerCallbacks();
  auto server = Server::create("127.0.0.1", 2404);
  auto cb = getCallable("_srv_unexpected");
  server->setOnUnexpectedMessageCallback(cb);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_ACTIVATION, 0,
                                      1, false, false);
  InformationObject io = (InformationObject)SinglePointInformation_create(
      nullptr, 100, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message = Remote::Message::IncomingMessage::create(asdu, &params);
  server->onUnexpectedMessage(mock, message, INVALID_COT);
  std::this_thread::sleep_for(std::chrono::milliseconds(20));

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}

// ---------------------------------------------------------------------------
// getValidMessage
// ---------------------------------------------------------------------------

TEST_CASE("Server: getValidMessage valid / invalid CA / invalid COT",
          "[server][validmsg]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // valid message (CA=1 exists, command type with ACTIVATION)
  CS101_ASDU valid = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11,
                                     1, CS101_COT_ACTIVATION, 5, false);
  auto vmsg = server->getValidMessage(mock, valid);
  REQUIRE(vmsg != nullptr);
  REQUIRE(vmsg->getOriginatorAddress() == 5);
  CS101_ASDU_destroy(valid);

  // unknown CA -> UNKNOWN_CA, nullptr
  CS101_ASDU badca = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11,
                                     999, CS101_COT_ACTIVATION, 0, false);
  auto bmsg = server->getValidMessage(mock, badca);
  REQUIRE(bmsg.get() == nullptr);
  CS101_ASDU_destroy(badca);

  // invalid COT (unsupported combo on a monitoring type drives INVALID_COT)
  CS101_ASDU badcot = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 11,
                                     CS101_COT_ACTIVATION_CON, 0, 1);
  InformationObject io = (InformationObject)SinglePointInformation_create(
      nullptr, 11, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU_addInformationObject(badcot, io);
  auto cmsg = server->getValidMessage(mock, badcot);
  REQUIRE(cmsg.get() == nullptr);
  InformationObject_destroy(io);
  CS101_ASDU_destroy(badcot);
}

// ---------------------------------------------------------------------------
// start / stop / lifecycle + isRunning/hasOpenConnections
// ---------------------------------------------------------------------------

TEST_CASE("Server: start/stop lifecycle", "[server][lifecycle]") {
  auto server = Server::create("127.0.0.1", 2406, 50);
  REQUIRE(server->isRunning() == false);
  REQUIRE_NOTHROW(server->start());
  REQUIRE(server->isRunning() == true);
  // second start is a no-op (already enabled)
  REQUIRE_NOTHROW(server->start());
  REQUIRE_NOTHROW(server->stop());
  REQUIRE(server->isRunning() == false);
  // second stop is a no-op
  REQUIRE_NOTHROW(server->stop());
}

TEST_CASE("Server: open connection counts without clients",
          "[server][counts]") {
  auto server = Server::create("127.0.0.1", 2407, 50);
  REQUIRE(server->hasOpenConnections() == false);
  REQUIRE(server->getOpenConnectionCount() == 0);
  server->start();
  REQUIRE_NOTHROW(server->hasOpenConnections());
  server->stop();
}

// ---------------------------------------------------------------------------
// send / transmit / sendBatch
// ---------------------------------------------------------------------------

TEST_CASE("Server: send not enabled returns false", "[server][send]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_SP_NA_1);

  auto message = Remote::Message::PointMessage::create(point);
  // not enabled -> false
  REQUIRE(server->send(message) == false);
}

TEST_CASE("Server: transmit monitoring + control + high-priority send",
          "[server][send]") {
  auto server = Server::create("127.0.0.1", 2408, 50);
  auto station = server->addStation(1);
  auto mpoint = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);
  auto cpoint = station->addPoint(12, IEC60870_5_TypeID::C_SC_NA_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  server->start();
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);
  REQUIRE(server->hasActiveConnections() == true);

  // transmit monitoring point -> PointMessage, low-priority enqueue
  REQUIRE(server->transmit(mpoint, CS101_COT_SPONTANEOUS, 7) == true);
  // transmit control point -> PointCommand, low-priority enqueue
  REQUIRE(server->transmit(cpoint, CS101_COT_ACTIVATION_CON, 7) == true);

  // send high priority via mock connection (non-periodic, non-spontaneous)
  {
    auto msg = Remote::Message::PointMessage::create(mpoint);
    msg->setCauseOfTransmission(CS101_COT_REQUEST);
    REQUIRE(server->send(msg, mock) == true);
    REQUIRE(state.sendASDU_count == 1);
  }

  server->stop();
  // after stop -> enabled false, send returns false
  auto msg2 = Remote::Message::PointMessage::create(mpoint);
  REQUIRE(server->send(msg2) == false);
  REQUIRE(server->send(msg2, mock) == false);
}

TEST_CASE("Server: sendBatch branches", "[server][send]") {
  auto server = Server::create("127.0.0.1", 2409, 50);
  auto station = server->addStation(1);
  auto p1 = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);
  auto p2 = station->addPoint(12, IEC60870_5_TypeID::M_ME_NC_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  // not enabled -> false
  auto batch = Remote::Message::Batch::create(CS101_COT_SPONTANEOUS);
  batch->addPoint(p1);
  REQUIRE(server->sendBatch(batch) == false);

  server->start();
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  // empty batch -> false (after enabled+active)
  auto empty = Remote::Message::Batch::create(CS101_COT_SPONTANEOUS);
  REQUIRE(server->sendBatch(empty) == false);

  // valid batch, low priority (connection null)
  auto b1 = Remote::Message::Batch::create(CS101_COT_SPONTANEOUS);
  b1->addPoint(p1);
  b1->addPoint(p2);
  REQUIRE(server->sendBatch(b1) == true);

  // valid batch, high priority to mock connection, PERIODIC ->
  // onBeforeAutoTransmit
  auto b2 = Remote::Message::Batch::create(CS101_COT_PERIODIC);
  b2->addPoint(p1);
  REQUIRE(server->sendBatch(b2, mock) == true);

  // non-periodic to mock -> onBeforeRead + high priority send
  auto b3 = Remote::Message::Batch::create(CS101_COT_REQUEST);
  b3->addPoint(p1);
  REQUIRE(server->sendBatch(b3, mock) == true);
  REQUIRE(state.sendASDU_count >= 1);

  server->stop();
}

// ---------------------------------------------------------------------------
// activation confirmation / termination
// ---------------------------------------------------------------------------

TEST_CASE("Server: sendActivationConfirmation / Termination",
          "[server][activation]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  auto station2 = server->addStation(2);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // not an existing connection -> early return
  CS101_ASDU a = Test::makeASDU(IEC60870_5_TypeID::C_SC_NA_1, 11,
                                CS101_COT_ACTIVATION, 0, 1);
  server->sendActivationConfirmation(mock, a);
  REQUIRE(state.sendASDU_count == 0);
  CS101_ASDU_destroy(a);

  // make connection existing
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  // non-global CA -> single send
  CS101_ASDU b = Test::makeASDU(IEC60870_5_TypeID::C_SC_NA_1, 11,
                                CS101_COT_ACTIVATION, 0, 1);
  server->sendActivationConfirmation(mock, b);
  REQUIRE(state.sendASDU_count == 1);
  CS101_ASDU_destroy(b);

  // global CA -> send to all stations
  CS101_ASDU g =
      Test::makeASDU(IEC60870_5_TypeID::C_SC_NA_1, 11, CS101_COT_ACTIVATION, 0,
                     IEC60870_GLOBAL_COMMON_ADDRESS);
  server->sendActivationConfirmation(mock, g);
  REQUIRE(state.sendASDU_count == 3);
  CS101_ASDU_destroy(g);

  // termination non-global
  CS101_ASDU t = Test::makeASDU(IEC60870_5_TypeID::C_SC_NA_1, 11,
                                CS101_COT_ACTIVATION, 0, 1);
  server->sendActivationTermination(mock, t);
  REQUIRE(state.sendASDU_count == 4);
  CS101_ASDU_destroy(t);

  // termination global
  CS101_ASDU tg =
      Test::makeASDU(IEC60870_5_TypeID::C_SC_NA_1, 11, CS101_COT_ACTIVATION, 0,
                     IEC60870_GLOBAL_COMMON_ADDRESS);
  server->sendActivationTermination(mock, tg);
  REQUIRE(state.sendASDU_count == 6);
  CS101_ASDU_destroy(tg);

  // termination on a non-existing connection -> early return
  Test::MockPeerState state2;
  sIPeerConnection peer2 = Test::makeMockPeer(state2);
  IMasterConnection mock2 = &peer2;
  server->sendActivationTermination(mock2, tg);
  REQUIRE(state2.sendASDU_count == 0);
}

// ---------------------------------------------------------------------------
// sendEndOfInitialization / sendPeriodicInventory
// ---------------------------------------------------------------------------

TEST_CASE("Server: sendEndOfInitialization", "[server][eoi]") {
  auto server = Server::create("127.0.0.1", 2410, 50);
  server->start();
  REQUIRE_NOTHROW(server->sendEndOfInitialization(
      1, CS101_CauseOfInitialization::LOCAL_POWER_ON));
  server->stop();
}

TEST_CASE("Server: sendPeriodicInventory no-conn / disabled", "[server][inv]") {
  auto server = Server::create("127.0.0.1", 2411, 50);
  auto station = server->addStation(1);
  auto p = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);

  // not enabled -> early return
  server->sendPeriodicInventory();

  server->start();
  // enabled but no active connections -> early return
  server->sendPeriodicInventory();
  server->stop();
}

TEST_CASE("Server: sendPeriodicInventory fills batch",
          "[server][inv][!mayfail]") {
  auto server = Server::create("127.0.0.1", 2412, 50);
  auto station = server->addStation(1);
  auto p = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;

  server->start();
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);
  // timing sensitive: periodic may or may not be due yet
  server->sendPeriodicInventory();
  server->sendPeriodicInventory();
  server->stop();
}

// ---------------------------------------------------------------------------
// interrogationHandler
// ---------------------------------------------------------------------------

TEST_CASE("Server: interrogationHandler null instance + non-station group",
          "[server][interrogation]") {
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 11,
                                   CS101_COT_ACTIVATION, 0, 1);
  REQUIRE(Server::interrogationHandler(dangling, mock, asdu,
                                       IEC60870_QOI_STATION) == false);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: interrogationHandler STATION group with points",
          "[server][interrogation]") {
  auto server = Server::create("127.0.0.1", 2413, 50);
  auto station = server->addStation(1);
  for (int i = 0; i < 5; i++) {
    station->addPoint(100 + i, IEC60870_5_TypeID::M_ME_NC_1);
  }

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = Test::makeASDU(IEC60870_5_TypeID::C_IC_NA_1, 11,
                                   CS101_COT_ACTIVATION, 123, 1);
  InformationObject io = (InformationObject)InterrogationCommand_create(
      nullptr, 11, IEC60870_QOI_STATION);
  CS101_ASDU_addInformationObject(asdu, io);
  REQUIRE(Server::interrogationHandler(server.get(), mock, asdu,
                                       IEC60870_QOI_STATION) == true);
  REQUIRE(state.sendASDU_count >= 1);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: interrogationHandler special (non-station) group",
          "[server][interrogation]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = Test::makeASDU(IEC60870_5_TypeID::C_IC_NA_1, 11,
                                   CS101_COT_ACTIVATION, 0, 1);
  InformationObject io = (InformationObject)InterrogationCommand_create(
      nullptr, 11, IEC60870_QOI_STATION);
  CS101_ASDU_addInformationObject(asdu, io);
  // group 1 -> negative confirmation + UNIMPLEMENTED_GROUP
  REQUIRE(Server::interrogationHandler(server.get(), mock, asdu,
                                       IEC60870_QOI_GROUP_1) == true);
  REQUIRE(state.sendASDU_count >= 1);
  CS101_ASDU_destroy(asdu);
}

// ---------------------------------------------------------------------------
// counterInterrogationHandler
// ---------------------------------------------------------------------------

TEST_CASE("Server: counterInterrogationHandler branches", "[server][counter]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  station->addPoint(11, IEC60870_5_TypeID::M_IT_NA_1); // binary counter

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // null instance -> false
  CS101_ASDU a = Test::makeASDU(IEC60870_5_TypeID::C_CI_NA_1, 11,
                                CS101_COT_ACTIVATION, 0, 1);
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  REQUIRE(Server::counterInterrogationHandler(dangling, mock, a, 0) == false);
  CS101_ASDU_destroy(a);

  // GENERAL request (qcc rqt=GENERAL, frz=READ) -> confirm + batch + term
  CS101_ASDU b = Test::makeASDU(IEC60870_5_TypeID::C_CI_NA_1, 11,
                                CS101_COT_ACTIVATION, 3, 1);
  InformationObject io =
      (InformationObject)CounterInterrogationCommand_create(nullptr, 11, 0);
  CS101_ASDU_addInformationObject(b, io);
  REQUIRE(Server::counterInterrogationHandler(server.get(), mock, b, 0) ==
          true);
  REQUIRE(state.sendASDU_count >= 1);
  CS101_ASDU_destroy(b);
}

TEST_CASE("Server: counterInterrogationHandler unsupported group",
          "[server][counter]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  station->addPoint(11, IEC60870_5_TypeID::M_IT_NA_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  // rqt=group1 (1), frz=READ(0) -> NOT GENERAL && frz==READ -> unsupported
  CS101_ASDU asdu = Test::makeASDU(IEC60870_5_TypeID::C_CI_NA_1, 11,
                                   CS101_COT_ACTIVATION, 0, 1);
  InformationObject io =
      (InformationObject)CounterInterrogationCommand_create(nullptr, 11, 1);
  CS101_ASDU_addInformationObject(asdu, io);
  uint8_t qcc = 1; // RQT_GROUP_1
  REQUIRE(Server::counterInterrogationHandler(server.get(), mock, asdu, qcc) ==
          true);
  REQUIRE(state.sendASDU_count >= 1);
  CS101_ASDU_destroy(asdu);
}

// ---------------------------------------------------------------------------
// readHandler
// ---------------------------------------------------------------------------

TEST_CASE("Server: readHandler read allowed monitoring point",
          "[server][read]") {
  auto server = Server::create("127.0.0.1", 2414, 50);
  auto station = server->addStation(1);
  auto p = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  server->start();
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu =
      Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 11, CS101_COT_REQUEST, 4, 1);
  REQUIRE(Server::readHandler(server.get(), mock, asdu, 11) == true);
  CS101_ASDU_destroy(asdu);

  server->stop();
}

TEST_CASE(
    "Server: readHandler null instance + unsupported type + unknown ioa/ca",
    "[server][read]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  station->addPoint(11,
                    IEC60870_5_TypeID::M_IT_NA_1); // counter read not allowed

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // null instance -> false
  CS101_ASDU a =
      Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 11, CS101_COT_REQUEST, 0, 1);
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  REQUIRE(Server::readHandler(dangling, mock, a, 11) == false);
  CS101_ASDU_destroy(a);

  // M_IT_NA_1 type (binary counter) -> INVALID_TYPE_ID (read not allowed)
  CS101_ASDU b =
      Test::makeASDU(IEC60870_5_TypeID::M_IT_NA_1, 11, CS101_COT_REQUEST, 0, 1);
  BinaryCounterReading bcr =
      BinaryCounterReading_create(nullptr, 0, 0, false, false, false);
  InformationObject io =
      (InformationObject)IntegratedTotals_create(nullptr, 11, bcr);
  CS101_ASDU_addInformationObject(b, io);
  REQUIRE(Server::readHandler(server.get(), mock, b, 11) == true);
  InformationObject_destroy(io);
  BinaryCounterReading_destroy(bcr);
  CS101_ASDU_destroy(b);

  // unknown IOA -> UNKNOWN_IOA
  CS101_ASDU c = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 999,
                                CS101_COT_REQUEST, 0, 1);
  InformationObject io2 = (InformationObject)SinglePointInformation_create(
      nullptr, 999, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU_addInformationObject(c, io2);
  REQUIRE(Server::readHandler(server.get(), mock, c, 999) == true);
  InformationObject_destroy(io2);
  CS101_ASDU_destroy(c);

  // unknown CA -> UNKNOWN_CA
  CS101_ASDU d = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 11,
                                CS101_COT_REQUEST, 0, 999);
  InformationObject io3 = (InformationObject)SinglePointInformation_create(
      nullptr, 11, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU_addInformationObject(d, io3);
  REQUIRE(Server::readHandler(server.get(), mock, d, 11) == true);
  InformationObject_destroy(io3);
  CS101_ASDU_destroy(d);
}

// ---------------------------------------------------------------------------
// asduHandler - clock sync, commands, select/execute, errors
// ---------------------------------------------------------------------------

TEST_CASE("Server: asduHandler null instance", "[server][asdu]") {
  void *dangling = reinterpret_cast<void *>(0xdeadbeef);
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = Test::makeASDU(IEC60870_5_TypeID::C_SC_NA_1, 11,
                                   CS101_COT_ACTIVATION, 0, 1);
  REQUIRE(Server::asduHandler(dangling, mock, asdu) == false);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: asduHandler clock sync (C_CS_NA_1)", "[server][asdu]") {
  ensureServerCallbacks();
  auto server = Server::create("127.0.0.1", 2415, 50);
  auto station = server->addStation(1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = makeCommandAsdu(params, IEC60870_5_TypeID::C_CS_NA_1, 1, 1,
                                    CS101_COT_ACTIVATION, 0, false);
  REQUIRE(Server::asduHandler(server.get(), mock, asdu) == true);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: asduHandler direct command execute success",
          "[server][asdu]") {
  auto server = Server::create("127.0.0.1", 2416, 50);
  auto station = server->addStation(1);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  // execute command (select=false) on DIRECT point -> success
  CS101_ASDU asdu = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11, 1,
                                    CS101_COT_ACTIVATION, 3, false);
  REQUIRE(Server::asduHandler(server.get(), mock, asdu) == true);
  REQUIRE(state.sendASDU_count >= 1);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: asduHandler select in DIRECT mode -> failure",
          "[server][asdu]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1); // DIRECT

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  // select command on DIRECT point -> failure (negative confirmation sent)
  CS101_ASDU asdu = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11, 1,
                                    CS101_COT_ACTIVATION, 3, true);
  REQUIRE(Server::asduHandler(server.get(), mock, asdu) == true);
  REQUIRE(state.sendASDU_count >= 1);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: asduHandler select-and-execute select + execute",
          "[server][asdu]") {
  auto server = Server::create("127.0.0.1", 2417, 50);
  auto station = server->addStation(1);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);
  point->setCommandMode(SELECT_AND_EXECUTE_COMMAND);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // select command on S&E point -> SUCCESS -> getSelector reflects selection
  CS101_ASDU s = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11, 1,
                                 CS101_COT_ACTIVATION, 9, true);
  REQUIRE(Server::asduHandler(server.get(), mock, s) == true);
  REQUIRE(server->getSelector(1, 11).has_value() == true);
  REQUIRE(server->getSelector(1, 11).value() == 9);
  CS101_ASDU_destroy(s);

  // execute command on S&E point (selected) -> success
  CS101_ASDU e = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11, 1,
                                 CS101_COT_ACTIVATION, 9, false);
  REQUIRE(Server::asduHandler(server.get(), mock, e) == true);
  CS101_ASDU_destroy(e);
}

TEST_CASE("Server: asduHandler S&E execute without selection -> failure",
          "[server][asdu]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_DC_NA_1);
  point->setCommandMode(SELECT_AND_EXECUTE_COMMAND);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  // execute without prior selection -> FAILURE
  CS101_ASDU asdu = makeCommandAsdu(params, IEC60870_5_TypeID::C_DC_NA_1, 11, 1,
                                    CS101_COT_ACTIVATION, 3, false);
  REQUIRE(Server::asduHandler(server.get(), mock, asdu) == true);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("Server: asduHandler mismatched type / unknown ioa / unknown ca / "
          "monitoring",
          "[server][asdu]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // mismatched type: point type C_SC_NA_1(45), message type C_DC_NA_1(46)
  CS101_ASDU m = makeCommandAsdu(params, IEC60870_5_TypeID::C_DC_NA_1, 11, 1,
                                 CS101_COT_ACTIVATION, 0, false);
  REQUIRE(Server::asduHandler(server.get(), mock, m) == true);
  CS101_ASDU_destroy(m);

  // unknown IOA (no point at 999)
  CS101_ASDU u = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 999, 1,
                                 CS101_COT_ACTIVATION, 0, false);
  REQUIRE(Server::asduHandler(server.get(), mock, u) == true);
  CS101_ASDU_destroy(u);

  // unknown CA (no station 999)
  CS101_ASDU c = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11, 999,
                                 CS101_COT_ACTIVATION, 0, false);
  REQUIRE(Server::asduHandler(server.get(), mock, c) == true);
  CS101_ASDU_destroy(c);

  // monitoring message type (not a command) -> INVALID_TYPE_ID
  CS101_ASDU mo = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 11,
                                 CS101_COT_SPONTANEOUS, 0, 1);
  InformationObject io = (InformationObject)SinglePointInformation_create(
      nullptr, 11, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU_addInformationObject(mo, io);
  REQUIRE(Server::asduHandler(server.get(), mock, mo) == true);
  InformationObject_destroy(io);
  CS101_ASDU_destroy(mo);
}

TEST_CASE("Server: asduHandler related auto return path", "[server][asdu]") {
  auto server = Server::create("127.0.0.1", 2418, 50);
  auto station = server->addStation(1);
  auto cmd = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);
  auto rel = station->addPoint(12, IEC60870_5_TypeID::M_SP_NA_1);
  cmd->setRelatedInformationObjectAddress(12);
  cmd->setRelatedInformationObjectAutoReturn(true);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();
  CS101_ASDU asdu = makeCommandAsdu(params, IEC60870_5_TypeID::C_SC_NA_1, 11, 1,
                                    CS101_COT_ACTIVATION, 3, false);
  REQUIRE(Server::asduHandler(server.get(), mock, asdu) == true);
  CS101_ASDU_destroy(asdu);
}

// ---------------------------------------------------------------------------
// getSelector / select edge + scheduler
// ---------------------------------------------------------------------------

TEST_CASE("Server: getSelector not found", "[server][selector]") {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);
  auto cmd = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);

  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);

  sCS101_AppLayerParameters params = Test::defaultAppLayerParameters();

  // getSelector not found
  REQUIRE(server->getSelector(1, 11).has_value() == false);
}

TEST_CASE("Server: scheduleTask / schedulePeriodicTask", "[server][sched]") {
  auto server = Server::create("127.0.0.1", 2404);
  int counter = 0;
  server->scheduleTask([&counter]() { counter++; }, 0);
  server->scheduleTask([&counter]() { counter++; },
                       -5); // negative -> immediate
  // interval < 50 throws
  REQUIRE_THROWS_AS(server->schedulePeriodicTask([]() {}, 49),
                    std::out_of_range);
  REQUIRE_NOTHROW(server->schedulePeriodicTask([]() {}, 100));
}

TEST_CASE("Server: scheduleDataPointTimer via server thread",
          "[server][timer][!mayfail]") {
  auto server = Server::create("127.0.0.1", 2419, 50);
  auto station = server->addStation(1);
  auto p = station->addPoint(11, IEC60870_5_TypeID::M_SP_NA_1);

  server->start();
  Test::MockPeerState state;
  sIPeerConnection peer = Test::makeMockPeer(state);
  IMasterConnection mock = &peer;
  Server::connectionEventHandler(server.get(), mock, CS104_CON_EVENT_ACTIVATED);
  // the periodic thread calls scheduleDataPointTimer automatically
  std::this_thread::sleep_for(std::chrono::milliseconds(30));
  server->stop();
}
