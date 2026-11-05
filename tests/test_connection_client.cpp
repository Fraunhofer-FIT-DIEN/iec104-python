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
 * @file test_connection_client.cpp
 * @brief Drives src/remote/Connection.cpp and src/Client.cpp toward high line
 * coverage WITHOUT live network traffic.
 *
 * Design notes:
 * - A Connection is created via Client::create() + addConnection(); it is fully
 *   usable (state machine, getters, stations, command-tracking, callbacks)
 *   without any socket being opened.
 * - The socket-dependent send path (command/read/transmit/test/clockSync/
 *   interrogation/counterInterrogation/mute/unmute/connect) is driven only
 *   through its NOT-open / early-return branches here. Live loopback success
 *   paths are left to test_loopback.cpp.
 * - State transitions are driven through setOpen()/setClosed()/disconnect()/
 *   connect() early returns and the static connectionHandler() so no real TCP
 *   socket is ever spawned.
 * - GIL: mirrors tests/main.cpp - the embedded interpreter must only ever be
 *   created once (main.cpp holds the py::scoped_interpreter).
 */

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include <chrono>
#include <thread>

#include "Client.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/Connection.h"
#include "remote/Helper.h"
#include "remote/TransportSecurity.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/PointCommand.h"
#include "types.h"

#include "mock_helpers.h"

namespace py = pybind11;
using namespace std::chrono_literals;

namespace {

// ---------------------------------------------------------------------------
// Python callbacks defined in the embedded interpreter. Signatures must match
// what the C++ callback plumbing expects. Wrapped so we never crash when the
// interpreter is unavailable (e.g. a build without embedding).
// ---------------------------------------------------------------------------
void ensureConnectionCallbacks() {
  try {
    py::exec(R"(
import c104

def _cb_state(connection: c104.Connection, state: c104.ConnectionState) -> None:
    pass

def _cb_raw(connection: c104.Connection, data: bytes) -> None:
    pass

def _cb_unexpected(connection: c104.Connection, message: c104.IncomingMessage, cause: c104.Umc) -> None:
    pass

def _cb_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
    pass

def _cb_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
    pass

def _cb_eoi(client: c104.Client, station: c104.Station, cause: c104.Coi) -> None:
    pass
)",
             py::globals());
  } catch (const std::exception &) {
    // interpreter unavailable - callback branches stay uncovered but nothing
    // may crash
  }
}

// Fetch a named callback as a py::object (lvalue, as the setters need).
py::object cb(const char *name) {
  try {
    return py::globals()[name];
  } catch (const std::exception &) {
    return py::none();
  }
}

// Helper to build an IncomingMessage carrying a single M_SP_NA_1 information
// object at the given IOA within the given common address. Used to drive the
// command-tracking helpers (setCommandSuccess).
std::shared_ptr<Remote::Message::IncomingMessage>
makeSinglePointMessage(uint_fast16_t ca, uint_fast32_t ioa,
                       CS101_CauseOfTransmission cot, bool negative = false) {
  auto alParams = Test::defaultAppLayerParameters();
  CS101_ASDU asdu =
      Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, ioa, cot, 0, ca, &alParams);
  CS101_ASDU_setNegative(asdu, negative);
  InformationObject io = (InformationObject)SinglePointInformation_create(
      nullptr, (int)ioa, true,
      negative ? static_cast<QualityDescriptor>(IEC60870_QUALITY_INVALID)
               : static_cast<QualityDescriptor>(IEC60870_QUALITY_GOOD));
  CS101_ASDU_addInformationObject(asdu, io);
  auto msg = Remote::Message::IncomingMessage::create(asdu, &alParams);
  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
  return msg;
}

} // namespace

// ===========================================================================
// Connection construction / validation
// ===========================================================================

TEST_CASE("Client create / Connection create lifecycle",
          "[client][connection]") {
  auto client = Client::create();
  REQUIRE(client->getOriginatorAddress() == 0);
  REQUIRE(client->getTickRate_ms() == 100);
  REQUIRE(client->isRunning() == false);
  REQUIRE(client->hasConnections() == false);
  REQUIRE(client->getConnections().empty());
  REQUIRE(client->toString().find("<104.Client") != std::string::npos);
}

TEST_CASE("Connection keeps TransportSecurity alive after Client is gone",
          "[client][connection][security]") {
  // SECURITY.md H3: lib60870 keeps a raw TLSConfiguration pointer for the
  // whole lifetime of the CS104_Connection, so the configuration must be
  // co-owned by the Connection and not only by the Client.
  std::weak_ptr<Remote::TransportSecurity> weak_security;
  std::shared_ptr<Remote::Connection> con;
  {
    auto security = Remote::TransportSecurity::create(false, false);
    weak_security = security;
    auto client = Client::create(100, 1000, security);
    con = client->addConnection("127.0.0.1", 2404, INIT_NONE);
    REQUIRE(con);
  } // client and the local security reference are gone here

  REQUIRE(con->getClient() == nullptr);
  REQUIRE_FALSE(weak_security.expired());

  con.reset();
  REQUIRE(weak_security.expired());
}

TEST_CASE("Client create throws on tick_rate below 50", "[client]") {
  REQUIRE_THROWS_AS(Client::create(49), std::range_error);
  REQUIRE_THROWS_AS(Client::create(0), std::range_error);
}

TEST_CASE("Connection create with invalid IP throws", "[connection]") {
  auto client = Client::create();
  REQUIRE_THROWS_AS(client->addConnection("not-an-ip", 2404),
                    std::invalid_argument);
  REQUIRE_THROWS_AS(client->addConnection("256.1.1.1", 2404),
                    std::invalid_argument);
}

TEST_CASE("Connection create with invalid port throws", "[connection]") {
  auto client = Client::create();
  REQUIRE_THROWS_AS(client->addConnection("127.0.0.1", 0),
                    std::invalid_argument);
  // 65535 is a valid port (SECURITY.md L12), the uint16 parameter cannot
  // carry anything larger
  REQUIRE(client->addConnection("127.0.0.1", 65535) != nullptr);
}

TEST_CASE("Connection basic getters", "[connection]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  REQUIRE(conn->getIP() == "127.0.0.1");
  REQUIRE(conn->getPort() == 2404);
  REQUIRE(conn->getConnectionString() == "127.0.0.1:2404");
  REQUIRE(conn->getClient().get() == client.get());
  REQUIRE(conn->getState() == ConnectionState::CLOSED);
  REQUIRE(conn->getOriginatorAddress() == 0);
  REQUIRE(conn->isOpen() == false);
  REQUIRE(conn->isMuted() == false);
  REQUIRE(conn->getConnectedAt().has_value() == false);
  // a closed connection that was never explicitly opened still yields a value
  // (the default-constructed epoch time_point) from getDisconnectedAt()
  REQUIRE(conn->getDisconnectedAt().has_value() == true);
  REQUIRE(conn->getParameters() != nullptr);
  REQUIRE(conn->toString().find("<104.Connection") != std::string::npos);
}

TEST_CASE("Connection create with custom originator address", "[connection]") {
  auto client = Client::create();
  // originator address is applied via Connection::create default arg - here we
  // verify that setOriginatorAddress keeps the connection usable without a
  // live socket (it only touches the lib60870 app-layer parameters).
  auto conn = client->addConnection("localhost", 2405);
  REQUIRE(conn->getConnectionString() == "localhost:2405");
  client->setOriginatorAddress(7);
  REQUIRE(client->getOriginatorAddress() == 7);
  REQUIRE(conn->getOriginatorAddress() == 7);
  client->setOriginatorAddress(7); // no-op path (prev == address)
  REQUIRE(conn->getOriginatorAddress() == 7);
}

// ===========================================================================
// Connection state machine (no sockets)
// ===========================================================================

TEST_CASE("Connection connect/disconnect early returns",
          "[connection][state]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  // connect() on CLOSED would spawn a real socket; drive instead the
  // early-return branches by first forcing OPEN and OPEN_AWAIT_CLOSED states.
  conn->setOpen(); // CLOSED -> OPEN_MUTED (no socket; schedules unmute task)
  REQUIRE(conn->isOpen() == true);
  REQUIRE(conn->isMuted() == true);
  REQUIRE(conn->getState() == ConnectionState::OPEN_MUTED);
  REQUIRE(conn->getConnectedAt().has_value() == true);

  // connect() when already OPEN_MUTED -> early return (no socket)
  conn->connect();
  REQUIRE(conn->getState() == ConnectionState::OPEN_MUTED);

  // disconnect() from OPEN_MUTED -> OPEN_AWAIT_CLOSED (closes lib60870 conn)
  conn->disconnect();
  REQUIRE(conn->getState() == ConnectionState::OPEN_AWAIT_CLOSED);

  // connect() when OPEN_AWAIT_CLOSED -> CLOSED_AWAIT_OPEN (early return)
  conn->connect();
  REQUIRE(conn->getState() == ConnectionState::CLOSED_AWAIT_OPEN);

  // connect() again while CLOSED_AWAIT_OPEN -> early return
  conn->connect();
  REQUIRE(conn->getState() == ConnectionState::CLOSED_AWAIT_OPEN);

  // disconnect() while CLOSED_AWAIT_OPEN -> OPEN_AWAIT_CLOSED (early return)
  conn->disconnect();
  REQUIRE(conn->getState() == ConnectionState::OPEN_AWAIT_CLOSED);

  // disconnect() while OPEN_AWAIT_CLOSED -> early return
  conn->disconnect();
  REQUIRE(conn->getState() == ConnectionState::OPEN_AWAIT_CLOSED);
}

TEST_CASE("Connection setClosed paths", "[connection][state]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  // setClosed() on CLOSED -> already closed early return
  conn->setClosed();
  REQUIRE(conn->getState() == ConnectionState::CLOSED);
  // closed (never explicitly opened) connection still reports a disconnect
  // time: getDisconnectedAt returns the default epoch time_point
  REQUIRE(conn->getDisconnectedAt().has_value() == true);

  // setOpen then setClosed -> schedules reconnect task, state
  // CLOSED_AWAIT_RECONNECT
  conn->setOpen();
  REQUIRE(conn->getState() == ConnectionState::OPEN_MUTED);
  conn->setClosed(); // not OPEN_AWAIT_CLOSED -> CLOSED_AWAIT_RECONNECT
  REQUIRE(conn->getState() == ConnectionState::CLOSED_AWAIT_RECONNECT);
  REQUIRE(conn->getDisconnectedAt().has_value() == true);

  // disconnect() from CLOSED_AWAIT_RECONNECT -> CLOSED (no socket)
  conn->disconnect();
  REQUIRE(conn->getState() == ConnectionState::CLOSED);

  // setOpen after a full round trip works again
  conn->setOpen();
  REQUIRE(conn->isOpen() == true);
  // controlled close path: OPEN -> setClosed while OPEN_AWAIT_CLOSED
  conn->disconnect(); // OPEN_MUTED -> OPEN_AWAIT_CLOSED
  conn->setClosed();  // OPEN_AWAIT_CLOSED -> CLOSED
  REQUIRE(conn->getState() == ConnectionState::CLOSED);
}

TEST_CASE("Connection connectionHandler drives states", "[connection][state]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  void *key = conn->getCallbackKey();

  // OPENED -> setOpen() -> OPEN_MUTED
  Remote::Connection::connectionHandler(key, nullptr, CS104_CONNECTION_OPENED);
  REQUIRE(conn->isOpen() == true);

  // CLOSED -> setClosed() -> CLOSED_AWAIT_RECONNECT
  Remote::Connection::connectionHandler(key, nullptr, CS104_CONNECTION_CLOSED);
  REQUIRE(conn->getState() == ConnectionState::CLOSED_AWAIT_RECONNECT);

  // FAILED -> setClosed()
  Remote::Connection::connectionHandler(key, nullptr, CS104_CONNECTION_FAILED);
  REQUIRE(conn->getState() == ConnectionState::CLOSED_AWAIT_RECONNECT);
}

TEST_CASE("Connection static handlers with dead key do not crash",
          "[connection][state]") {
  void *dead = reinterpret_cast<void *>(0x1234);
  uint8_t buf[8] = {0};
  Remote::Connection::rawMessageHandler(dead, buf, 8, true);
  Remote::Connection::rawMessageHandler(dead, buf, 8, false);
  Remote::Connection::connectionHandler(dead, nullptr, CS104_CONNECTION_OPENED);
  auto asdu = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 1,
                             CS101_COT_ACTIVATION, 0, 1);
  bool handled = Remote::Connection::asduHandler(dead, 0, asdu);
  REQUIRE(handled == false);
  CS101_ASDU_destroy(asdu);
}

// ===========================================================================
// Connection stations
// ===========================================================================

TEST_CASE("Connection station management", "[connection][station]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  REQUIRE(conn->hasStations() == false);
  REQUIRE(conn->getStations().empty());
  REQUIRE(conn->getStation(1).get() == nullptr);
  REQUIRE(conn->hasStation(1) == false);

  // global common address never resolves to a station
  REQUIRE(conn->getStation(IEC60870_GLOBAL_COMMON_ADDRESS).get() == nullptr);

  auto st = conn->addStation(1);
  REQUIRE(st.get() != nullptr);
  REQUIRE(conn->hasStations() == true);
  REQUIRE(conn->getStations().size() == 1);
  REQUIRE(conn->getStation(1).get() == st.get());
  REQUIRE(conn->hasStation(1) == true);

  // duplicate add returns nullptr
  REQUIRE(conn->addStation(1).get() == nullptr);

  // different common address -> different station
  auto st2 = conn->addStation(2);
  REQUIRE(st2.get() != nullptr);
  REQUIRE(conn->getStations().size() == 2);

  // remove existing
  REQUIRE(conn->removeStation(1) == true);
  REQUIRE(conn->hasStation(1) == false);
  REQUIRE(conn->getStations().size() == 1);

  // remove non-existing -> false
  REQUIRE(conn->removeStation(99) == false);

  // client view
  REQUIRE(client->getConnectionFromCommonAddress(2).get() == conn.get());
  REQUIRE(client->getConnectionFromCommonAddress(99).get() == nullptr);
}

TEST_CASE("Connection setMuted and init variants", "[connection][state]") {
  // INIT_ALL: setMuted(false) from OPEN_MUTED schedules interrogation+clockSync
  auto clientA = Client::create();
  auto connA = clientA->addConnection("127.0.0.1", 2404); // INIT_ALL
  connA->setOpen();                                       // OPEN_MUTED
  connA->setMuted(
      false); // schedules (queued) INTERROGATION+CLOCK_SYNC, no socket
  connA->setMuted(true); // no-op from OPEN_MUTED
  REQUIRE(connA->getState() == ConnectionState::OPEN_MUTED);

  // INIT_MUTED: setMuted(false) -> default branch -> setState(OPEN) (no socket)
  auto clientB = Client::create();
  auto connB = clientB->addConnection("127.0.0.1", 2405, INIT_MUTED);
  connB->setOpen(); // OPEN_MUTED (INIT_MUTED skips unmute scheduling)
  REQUIRE(connB->getState() == ConnectionState::OPEN_MUTED);
  connB->setMuted(false); // OPEN_MUTED && !value -> default -> OPEN
  REQUIRE(connB->getState() == ConnectionState::OPEN);
  REQUIRE(connB->isMuted() == false);
  // now muted again to cover OPEN && value -> OPEN_MUTED
  connB->setMuted(true);
  REQUIRE(connB->getState() == ConnectionState::OPEN_MUTED);

  // INIT_INTERROGATION / INIT_CLOCK_SYNC branches (setMuted(false) schedules)
  auto clientC = Client::create();
  auto connC = clientC->addConnection("127.0.0.1", 2406, INIT_INTERROGATION);
  connC->setOpen();
  connC->setMuted(false); // schedules INTERROGATION, no socket
  auto clientD = Client::create();
  auto connD = clientD->addConnection("127.0.0.1", 2407, INIT_CLOCK_SYNC);
  connD->setOpen();
  connD->setMuted(false); // schedules CLOCK_SYNC, no socket
}

TEST_CASE("Connection setOpen early-return and reconnect scheduling",
          "[connection][state]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  // setOpen already open -> early return
  conn->setOpen();
  conn->setOpen();
  REQUIRE(conn->isOpen() == true);

  // setClosed then setOpen again -> works (OPEN_MUTED again) and reconnect
  // scheduling path via setClosed() was already covered; here ensure the
  // disconnect-task path of setOpen is reachable by going OPEN then
  // OPEN_AWAIT_CLOSED then setOpen
  conn->setClosed(); // OPEN_MUTED -> CLOSED_AWAIT_RECONNECT (schedules connect)
  conn->setOpen();   // CLOSED_AWAIT_RECONNECT -> OPEN_MUTED
  REQUIRE(conn->isOpen() == true);
}

// ===========================================================================
// Connection command-tracking helpers (no sockets)
// ===========================================================================

TEST_CASE("Connection prepare/await/cancel command success",
          "[connection][command]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  // prepare then cancel (no wait)
  conn->prepareCommandSuccess("a", COMMAND_AWAIT_CON);
  conn->cancelCommandSuccess("a");

  // prepare duplicate -> runtime_error
  conn->prepareCommandSuccess("b", COMMAND_AWAIT_CON);
  REQUIRE_THROWS_AS(conn->prepareCommandSuccess("b", COMMAND_AWAIT_CON),
                    std::runtime_error);
  conn->cancelCommandSuccess("b");

  // cancel non-existent is a no-op
  conn->cancelCommandSuccess("zzz");

  // await on non-existent cmd -> returns false immediately
  REQUIRE(conn->awaitCommandSuccess("zzz") == false);
}

TEST_CASE("Connection setCommandSuccess drives result states",
          "[connection][command]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  conn->addStation(7);

  // Each case: prepare a cmdId in a particular state, then feed a matching
  // message so setCommandSuccess executes the relevant switch branch. We do not
  // await here (a condition-variable signal is lost if it arrives before the
  // wait, and awaiting would block for the full timeout); the await success
  // path is covered in a dedicated threaded test below.

  // COMMAND_AWAIT_CON + positive ACTIVATION_CON -> SUCCESS
  conn->prepareCommandSuccess("7-M_SP_NA_1-331", COMMAND_AWAIT_CON);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 331, CS101_COT_ACTIVATION_CON));

  // COMMAND_AWAIT_CON + negative -> FAILURE
  conn->prepareCommandSuccess("7-M_SP_NA_1-332", COMMAND_AWAIT_CON);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 332, CS101_COT_ACTIVATION_CON, true));

  // COMMAND_AWAIT_TERM + TERMINATION -> SUCCESS
  conn->prepareCommandSuccess("7-M_SP_NA_1-333", COMMAND_AWAIT_TERM);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 333, CS101_COT_ACTIVATION_TERMINATION));

  // COMMAND_AWAIT_CON_TERM: CON thrice then TERM thrice -> SUCCESS
  conn->prepareCommandSuccess("7-M_SP_NA_1-334", COMMAND_AWAIT_CON_TERM);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 334, CS101_COT_ACTIVATION_CON));
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 334, CS101_COT_ACTIVATION_CON));
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 334, CS101_COT_ACTIVATION_CON));
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 334, CS101_COT_ACTIVATION_TERMINATION));
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 334, CS101_COT_ACTIVATION_TERMINATION));
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 334, CS101_COT_ACTIVATION_TERMINATION));

  // COMMAND_AWAIT_CON_TERM: TERM before CON -> FAILURE
  conn->prepareCommandSuccess("7-M_SP_NA_1-335", COMMAND_AWAIT_CON_TERM);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 335, CS101_COT_ACTIVATION_TERMINATION));

  // COMMAND_AWAIT_CON_TERM: unknown COT -> FAILURE
  conn->prepareCommandSuccess("7-M_SP_NA_1-337", COMMAND_AWAIT_CON_TERM);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 337, CS101_COT_SPONTANEOUS));

  // COMMAND_AWAIT_REQUEST + REQUEST -> SUCCESS (read fix: type -> C_RD_NA_1)
  conn->prepareCommandSuccess("7-C_RD_NA_1-336", COMMAND_AWAIT_REQUEST);
  conn->setCommandSuccess(makeSinglePointMessage(7, 336, CS101_COT_REQUEST));

  // global-common-address fallback: prepare idAlt, message with a different CA
  conn->prepareCommandSuccess(std::to_string(IEC60870_GLOBAL_COMMON_ADDRESS) +
                                  "-M_SP_NA_1-340",
                              COMMAND_AWAIT_CON);
  conn->setCommandSuccess(
      makeSinglePointMessage(8, 340, CS101_COT_ACTIVATION_CON));

  // unmatched message (found = false path): nothing prepared, no-op
  conn->setCommandSuccess(makeSinglePointMessage(7, 999, CS101_COT_ACTIVATION));
}

TEST_CASE("Connection awaitCommandSuccess success via async notify",
          "[connection][command]") {
  // A 3s command-timeout acts as a safety valve so a misbehaving waiter can
  // never hang the whole suite (it would just return false after 3s).
  auto client = Client::create(100, 3000);
  auto conn = client->addConnection("127.0.0.1", 2404);
  conn->addStation(7);

  const std::string id = "7-M_SP_NA_1-400";
  conn->prepareCommandSuccess(id, COMMAND_AWAIT_CON);

  std::atomic<bool> result{false};
  std::thread waiter([&]() { result = conn->awaitCommandSuccess(id); });
  // give the waiter thread time to block inside wait_until before we notify
  std::this_thread::sleep_for(100ms);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 400, CS101_COT_ACTIVATION_CON));
  waiter.join();
  REQUIRE(result == true);

  // wait path that resolves to COMMAND_FAILURE (negative response)
  const std::string id2 = "7-M_SP_NA_1-401";
  conn->prepareCommandSuccess(id2, COMMAND_AWAIT_CON);
  std::atomic<bool> result2{false};
  std::thread waiter2([&]() { result2 = conn->awaitCommandSuccess(id2); });
  std::this_thread::sleep_for(100ms);
  conn->setCommandSuccess(
      makeSinglePointMessage(7, 401, CS101_COT_ACTIVATION_CON, true));
  waiter2.join();
  REQUIRE(result2 == false);
}

// ===========================================================================
// Connection send-path NOT-open branches
// ===========================================================================

TEST_CASE("Connection send commands return false when not open",
          "[connection][send]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  REQUIRE(conn->interrogation(1) == false);
  REQUIRE(conn->counterInterrogation(1) == false);
  REQUIRE(conn->clockSync(1) == false);
  REQUIRE(conn->test(1, true) == false);
  REQUIRE(conn->test(1, false) == false);
  REQUIRE(conn->mute() == false);
  REQUIRE(conn->unmute() == false);
}

TEST_CASE("Connection read returns false when not open / no station",
          "[connection][send]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  auto point = Test::makeDataPoint(IEC60870_5_TypeID::M_SP_NA_1, 42, 11);
  // not open -> false
  REQUIRE(conn->read(point) == false);
}

TEST_CASE("Connection transmit throws on invalid point type",
          "[connection][send]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  auto monitoringPoint =
      Test::makeDataPoint(IEC60870_5_TypeID::M_SP_NA_1, 42, 11);
  REQUIRE_THROWS_AS(conn->transmit(monitoringPoint, CS101_COT_ACTIVATION),
                    std::invalid_argument);
}

TEST_CASE("Connection command returns false when not open",
          "[connection][send]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  // build an outgoing control command from a point on a DIFFERENT server; the
  // connection is not open so command() returns false without sending.
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(10);
  auto ctrlPoint = station->addPoint(11, IEC60870_5_TypeID::C_SC_NA_1);
  ctrlPoint->setCommandMode(SELECT_AND_EXECUTE_COMMAND);
  REQUIRE(ctrlPoint->getCommandMode() == SELECT_AND_EXECUTE_COMMAND);

  auto msg = Remote::Message::PointCommand::create(ctrlPoint, false);
  REQUIRE(conn->command(msg) == false);
  REQUIRE(conn->command(msg, false) == false);
  REQUIRE(conn->command(msg, true, COMMAND_AWAIT_CON_TERM) == false);

  // transmit on a control point (not open -> command() returns false)
  REQUIRE(conn->transmit(ctrlPoint, CS101_COT_ACTIVATION) == false);
}

TEST_CASE("Connection awaitCommandSuccess timeout branch",
          "[connection][command]") {
  // A small client command-timeout lets us reach the wait_until timeout path
  // without waiting 10s.
  auto client = Client::create(100, 50); // tick 100, command timeout 50ms
  auto conn = client->addConnection("127.0.0.1", 2404);

  conn->prepareCommandSuccess("timeout-id", COMMAND_AWAIT_CON);
  // nothing notifies / sets a result -> wait_until times out after ~50ms
  REQUIRE(conn->awaitCommandSuccess("timeout-id") == false);
}

TEST_CASE("Client reconnectAll does not spawn sockets when closed",
          "[client][connection]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  conn->setOpen(); // OPEN_MUTED (no socket)
  REQUIRE(conn->isOpen() == true);

  client->reconnectAll();
  // reapconnect path: disconnect(OPEN_MUTED->OPEN_AWAIT_CLOSED) then
  // connect(OPEN_AWAIT_CLOSED->CLOSED_AWAIT_OPEN, early return, no socket)
  REQUIRE(conn->getState() == ConnectionState::CLOSED_AWAIT_OPEN);
}

// ===========================================================================
// Client connection management
// ===========================================================================

TEST_CASE("Client connection management", "[client][connection]") {
  auto client = Client::create();
  auto c1 = client->addConnection("127.0.0.1", 2404);
  auto c2 = client->addConnection("127.0.0.1", 2405);

  REQUIRE(client->hasConnections() == true);
  REQUIRE(client->getConnections().size() == 2);
  REQUIRE(client->hasConnection("127.0.0.1", 2404) == true);
  REQUIRE(client->hasConnection("127.0.0.1", 9999) == false);
  REQUIRE(client->getConnection("127.0.0.1", 2404).get() == c1.get());
  REQUIRE(client->getConnection("127.0.0.1", 2405).get() == c2.get());
  REQUIRE(client->getConnection("127.0.0.1", 9999).get() == nullptr);

  // duplicate add -> nullptr
  REQUIRE(client->addConnection("127.0.0.1", 2404).get() == nullptr);

  // open/active counts
  REQUIRE(client->hasOpenConnections() == false);
  REQUIRE(client->getOpenConnectionCount() == 0);
  REQUIRE(client->hasActiveConnections() == false);
  REQUIRE(client->getActiveConnectionCount() == 0);

  // open one connection -> folded counts
  c1->setOpen();
  REQUIRE(client->hasOpenConnections() == true);
  REQUIRE(client->getOpenConnectionCount() == 1);
  // note: c1 is OPEN_MUTED (not active)
  REQUIRE(client->hasActiveConnections() == false);
  REQUIRE(client->getActiveConnectionCount() == 0);

  // getConnectionFromString is exercised via getConnection
  REQUIRE(client->getConnection(c1->getIP(), c1->getPort()).get() == c1.get());

  // disconnectAll
  client->disconnectAll();
  REQUIRE(c1->isOpen() == false);
}

// ===========================================================================
// Client lifecycle (start/stop without any connection -> no sockets)
// ===========================================================================

TEST_CASE("Client start/stop lifecycle is idempotent", "[client][lifecycle]") {
  auto client = Client::create();
  REQUIRE(client->isRunning() == false);

  client->start();
  REQUIRE(client->isRunning() == true);
  client->start(); // already running -> no-op
  REQUIRE(client->isRunning() == true);

  client->stop();
  REQUIRE(client->isRunning() == false);
  client->stop(); // already stopped -> no-op
  REQUIRE(client->isRunning() == false);

  // restart is fine
  client->start();
  REQUIRE(client->isRunning() == true);
  client->stop();
  REQUIRE(client->isRunning() == false);
}

TEST_CASE("Client scheduleTask / schedulePeriodicTask validation",
          "[client][task]") {
  auto client = Client::create();
  bool ran = false;
  client->scheduleTask([&ran]() { ran = true; });
  client->scheduleTask([&ran]() { ran = true; }, -5); // immediate priority
  client->scheduleTask([&ran]() { ran = true; }, 50);

  // periodic interval below 50ms -> out_of_range
  REQUIRE_THROWS_AS(client->schedulePeriodicTask([&ran]() { ran = true; }, 49),
                    std::out_of_range);
  // valid periodic schedules a first task without throwing
  REQUIRE_NOTHROW(client->schedulePeriodicTask([&ran]() { ran = true; }, 100));
  (void)ran;
}

// ===========================================================================
// Client callback plumbing
// ===========================================================================

TEST_CASE("Client onNewStation default and callback routing",
          "[client][callback]") {
  ensureConnectionCallbacks();
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  // default behaviour (no callback set): adds the station
  REQUIRE(conn->hasStation(3) == false);
  client->onNewStation(conn, 3);
  REQUIRE(conn->hasStation(3) == true);

  // callback routing with a registered python callback
  py::object stationCb = cb("_cb_new_station");
  if (!stationCb.is_none()) {
    client->setOnNewStationCallback(stationCb);
    client->onNewStation(conn, 4); // callback path (no station added)
    REQUIRE(conn->hasStation(4) == false);
  }
}

TEST_CASE("Client onNewPoint default and callback routing",
          "[client][callback]") {
  ensureConnectionCallbacks();
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  auto station = conn->addStation(5);

  // default behaviour adds the point
  REQUIRE(station->getPoint(11).get() == nullptr);
  client->onNewPoint(station, 11, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(station->getPoint(11).get() != nullptr);

  py::object pointCb = cb("_cb_new_point");
  if (!pointCb.is_none()) {
    client->setOnNewPointCallback(pointCb);
    client->onNewPoint(station, 12, IEC60870_5_TypeID::M_ME_NC_1);
    REQUIRE(station->getPoint(12).get() == nullptr); // callback handled it
  }
}

TEST_CASE("Client onEndOfInitialization callback routing",
          "[client][callback]") {
  ensureConnectionCallbacks();
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  auto station = conn->addStation(6);

  py::object eoiCb = cb("_cb_eoi");
  if (!eoiCb.is_none()) {
    client->setOnEndOfInitializationCallback(eoiCb);
    client->onEndOfInitialization(station,
                                  CS101_CauseOfInitialization::LOCAL_POWER_ON);
  }
}

// ===========================================================================
// Connection callback plumbing (registers callbacks + drives scheduling)
// ===========================================================================

TEST_CASE("Connection setOn* callback registration", "[connection][callback]") {
  ensureConnectionCallbacks();
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  py::object stateCb = cb("_cb_state");
  py::object rawCb = cb("_cb_raw");
  py::object unexpCb = cb("_cb_unexpected");
  if (!stateCb.is_none())
    conn->setOnStateChangeCallback(stateCb);
  if (!rawCb.is_none())
    conn->setOnReceiveRawCallback(rawCb);
  if (!rawCb.is_none())
    conn->setOnSendRawCallback(rawCb);
  if (!unexpCb.is_none())
    conn->setOnUnexpectedMessageCallback(unexpCb);
}

TEST_CASE("Connection onReceiveRaw/onSendRaw/onUnexpectedMessage schedule",
          "[connection][callback]") {
  ensureConnectionCallbacks();
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);

  unsigned char buf[4] = {0x68, 0x02, 0x00, 0x00};
  // without callback registered these are no-ops
  conn->onReceiveRaw(buf, 4);
  conn->onSendRaw(buf, 4);
  auto msg = makeSinglePointMessage(1, 1, CS101_COT_ACTIVATION);
  conn->onUnexpectedMessage(msg, UNKNOWN_CA);

  // with callbacks registered, they schedule tasks on the client (no socket,
  // no thread needed for scheduling itself)
  py::object rawCb = cb("_cb_raw");
  py::object unexpCb = cb("_cb_unexpected");
  if (!rawCb.is_none()) {
    conn->setOnReceiveRawCallback(rawCb);
    conn->onReceiveRaw(buf, 4);
    conn->setOnSendRawCallback(rawCb);
    conn->onSendRaw(buf, 4);
  }
  if (!unexpCb.is_none()) {
    conn->setOnUnexpectedMessageCallback(unexpCb);
    conn->onUnexpectedMessage(msg, UNKNOWN_IOA);
    conn->onUnexpectedMessage(msg, INVALID_COT);
    conn->onUnexpectedMessage(msg, INVALID_TYPE_ID);
    conn->onUnexpectedMessage(msg, MISMATCHED_TYPE_ID);
  }
}

// ===========================================================================
// Connection asduHandler branches
// ===========================================================================

TEST_CASE("Connection asduHandler: client not running -> drop",
          "[connection][asdu]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404);
  conn->setOpen(); // isOpen, but client is NOT running
  void *key = conn->getCallbackKey();

  auto asdu = Test::makeASDU(IEC60870_5_TypeID::M_SP_NA_1, 1,
                             CS101_COT_SPONTANEOUS, 0, 3);
  // client->isRunning() is false -> throw "Client not running" -> drop
  bool handled = Remote::Connection::asduHandler(key, 0, asdu);
  REQUIRE(handled == false);
  CS101_ASDU_destroy(asdu);
}

// M13: a peer that repeats STARTDT_CON must not be able to queue one
// initialization task per frame. The initialization is claimed once per
// opening and released when it runs or when the connection closes.
TEST_CASE("Connection STARTDT_CON flood queues a single initialization",
          "[security][medium][connection]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 2404); // INIT_ALL
  void *key = conn->getCallbackKey();

  // the client thread is not started, so scheduled tasks stay in the queue and
  // the flood can be measured without any timing dependency
  REQUIRE(client->isRunning() == false);

  conn->setOpen();
  REQUIRE(conn->getState() == OPEN_MUTED);
  REQUIRE(conn->isInitializationPending() == false);
  const std::size_t baseline = client->getPendingTaskCount();

  // the first STARTDT_CON claims the initialization and queues one task
  Remote::Connection::connectionHandler(key, nullptr,
                                        CS104_CONNECTION_STARTDT_CON_RECEIVED);
  REQUIRE(conn->isInitializationPending() == true);
  const std::size_t afterFirst = client->getPendingTaskCount();
  REQUIRE(afterFirst == baseline + 1);

  // the flood: repeating STARTDT_CON/STOPDT_CON pairs must neither queue more
  // tasks nor release the claim of the still pending initialization
  for (int i = 0; i < 500; ++i) {
    Remote::Connection::connectionHandler(
        key, nullptr, CS104_CONNECTION_STARTDT_CON_RECEIVED);
    Remote::Connection::connectionHandler(key, nullptr,
                                          CS104_CONNECTION_STOPDT_CON_RECEIVED);
  }
  REQUIRE(client->getPendingTaskCount() == afterFirst);
  REQUIRE(conn->isInitializationPending() == true);
  REQUIRE(conn->getState() == OPEN_MUTED);

  // closing releases the claim even though the queued task never ran
  conn->setClosed();
  REQUIRE(conn->isInitializationPending() == false);

  // the next opening starts a new generation that initializes again, once
  conn->setOpen();
  REQUIRE(conn->getState() == OPEN_MUTED);
  const std::size_t afterReopen = client->getPendingTaskCount();
  for (int i = 0; i < 100; ++i) {
    Remote::Connection::connectionHandler(
        key, nullptr, CS104_CONNECTION_STARTDT_CON_RECEIVED);
  }
  REQUIRE(conn->isInitializationPending() == true);
  REQUIRE(client->getPendingTaskCount() == afterReopen + 1);
}

// M7: setOriginatorAddress iterates a snapshot of `connections` taken under
// connections_mutex, so it can run concurrently with add/remove_connection
TEST_CASE("Client setOriginatorAddress races add/remove_connection safely",
          "[client][M7]") {
  auto client = Client::create();
  for (int i = 0; i < 8; ++i) {
    client->addConnection("127.0.0.1", 2410 + i, INIT_NONE);
  }

  std::atomic_bool stop{false};
  {
    py::gil_scoped_release release;
    std::thread churn([&]() {
      py::gil_scoped_acquire acquire;
      for (int round = 0; round < 50; ++round) {
        for (int i = 0; i < 8; ++i) {
          client->addConnection("127.0.0.2", 2430 + i, INIT_NONE);
        }
        for (int i = 0; i < 8; ++i) {
          client->removeConnection("127.0.0.2", 2430 + i);
        }
        py::gil_scoped_release yield;
      }
      stop.store(true);
    });

    std::uint_fast8_t oa = 1;
    while (!stop.load()) {
      client->setOriginatorAddress(oa);
      oa = (oa % 200) + 1;
    }
    churn.join();
    client->setOriginatorAddress(42);
  }

  for (const auto &c : client->getConnections()) {
    REQUIRE(c->getOriginatorAddress() == 42);
  }
  REQUIRE(client->getOriginatorAddress() == 42);
}
