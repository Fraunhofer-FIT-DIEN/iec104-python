/**
 * Copyright 2026-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file test_loopback.cpp
 * @brief LIVE Client<->Server loopback tests over real localhost TCP sockets.
 *
 * These tests drive the socket/thread I/O paths in src/Server.cpp,
 * src/remote/Connection.cpp and src/Client.cpp that cannot be reached through
 * mocks: Server::start()'s run thread + select loop, real connection
 * acceptance, interrogation/read/transmit/clock-sync round trips against a
 * genuine connected peer, the client's connect()/disconnect()/command()
 * success paths that require an OPEN state, the client thread_run /
 * reconnectAll / sendPeriodicInventory paths and the raw-message + state-change
 * handlers firing on real bytes.
 *
 * The socket handshake is fully ASYNC: every test spins with a generous timeout
 * polling connection state instead of assuming the connection is instant.
 * Each TEST_CASE uses its own port (avoiding TIME_WAIT collisions) and always
 * stops server + client in teardown.
 *
 * GIL: mirrors tests/main.cpp - an embedded interpreter is already alive for
 * the whole process; we only create py::objects (never a second interpreter).
 */

#include <catch2/catch_test_macros.hpp>

#include <pybind11/embed.h>    // py::exec for Python callback registration
#include <pybind11/pybind11.h> // embedded interpreter requirement (GIL paths)

#include <atomic>
#include <chrono>
#include <cstdio>
#include <functional>
#include <string>
#include <thread>
#include <vector>

#include "Client.h"
#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/Connection.h"
#include "remote/Helper.h"
#include "remote/TransportSecurity.h"
#include "types.h"

namespace py = pybind11;

using namespace std::chrono_literals;

namespace {

// NOTE: On the orchestrator's Linux build the certs live under tests/certs/;
// on a Windows mirror they resolve via the absolute fallback. Probe a few
// candidate locations so the TLS variant works regardless of run directory.
std::string resolveCertPath(const std::string &filename) {
  const std::vector<std::string> prefixes = {
      "tests/certs/",
      "../tests/certs/",
      "C:/Users/unkel/GIT/Fraunhofer-FIT-DIEN/iec104-python/tests/certs/",
      "/home/martin/iec104/tests/certs/",
  };
  for (const auto &prefix : prefixes) {
    std::string candidate = prefix + filename;
    if (std::FILE *f = std::fopen(candidate.c_str(), "r")) {
      std::fclose(f);
      return candidate;
    }
  }
  return prefixes.front() + filename;
}

/// Busy-wait until @p fn returns true or @p timeout_ms elapses.
/// Releases the GIL during polling so that background threads (lib60870
/// connection thread, client scheduler thread) can acquire the GIL to fire
/// Python callbacks.  Without this release the connection thread would deadlock
/// waiting for the GIL while the test main thread holds it in this busy loop.
template <typename Fn>
bool waitUntil(Fn &&fn, int timeout_ms, int step_ms = 20) {
  // Release the GIL for the entire poll loop — fn() only touches C++ objects.
  py::gil_scoped_release const gil_release;
  auto const deadline =
      std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout_ms);
  while (std::chrono::steady_clock::now() < deadline) {
    if (fn())
      return true;
    std::this_thread::sleep_for(std::chrono::milliseconds(step_ms));
  }
  return fn();
}

/// Register the Python callback globals used by the loopback tests. These are
/// defined in the embedded interpreter (tests/main.cpp holds the
/// py::scoped_interpreter). Signatures must match what each setter expects.
void ensurePythonCallbacks() {
  try {
    py::exec(R"(
import c104
import datetime as dt

def _loop_cb_on_connect_accept(server: c104.Server, ip: str) -> bool:
    return True

def _loop_cb_on_connect_reject(server: c104.Server, ip: str) -> bool:
    return False

def _loop_cb_on_clock_sync(server: c104.Server, ip: str, date_time: dt.datetime) -> c104.ResponseState:
    return c104.ResponseState.SUCCESS

def _loop_cb_recv_raw_server(server: c104.Server, data: bytes) -> None:
    pass

def _loop_cb_send_raw_server(server: c104.Server, data: bytes) -> None:
    pass

def _loop_cb_state_change(connection: c104.Connection, state: c104.ConnectionState) -> None:
    pass

def _loop_cb_recv_raw_con(connection: c104.Connection, data: bytes) -> None:
    pass

def _loop_cb_send_raw_con(connection: c104.Connection, data: bytes) -> None:
    pass

def _loop_cb_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
    pass

def _loop_cb_on_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
    pass

def _loop_cb_on_station_initialized(client: c104.Client, station: c104.Station, cause: c104.Coi) -> None:
    pass

def _loop_cb_pt_receive(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    return c104.ResponseState.SUCCESS

def _loop_cb_pt_before_read(point: c104.Point) -> None:
    pass

def _loop_cb_pt_before_auto_transmit(point: c104.Point) -> None:
    pass
)",
             py::globals());
  } catch (const std::exception &) {
    // interpreter unavailable - callback branches stay uncovered but nothing
    // may crash
  }
}

bool fileExists(const std::string &path) {
  if (std::FILE *f = std::fopen(path.c_str(), "r")) {
    std::fclose(f);
    return true;
  }
  return false;
}

/// A listening socket can only be bound once the previous slave was destroyed.
/// Called from waitUntil() with the GIL released; Server::create needs it.
bool portIsFree(uint_fast16_t port) {
  py::gil_scoped_acquire const gil;
  try {
    auto probe = Server::create("127.0.0.1", port, 50, 10000, 8);
    probe->start();
    probe->stop();
    return true;
  } catch (const std::exception &) {
    return false;
  }
}

/// Bind one of the registered python globals to a named lvalue and invoke @p
/// fn. The callback setters take a non-const `py::object &`, so the caller must
/// own a named local.
template <typename Fn> void withGlobal(const char *name, Fn &&fn) {
  try {
    py::object cb = py::globals()[name];
    if (!cb.is_none())
      fn(cb);
  } catch (const std::exception &) {
    // interpreter unavailable - ignore
  }
}

// ---------------------------------------------------------------------------
// Test ports (distinct per TEST_CASE to avoid TIME_WAIT collisions)
// ---------------------------------------------------------------------------
constexpr uint_fast16_t PORT_HANDSHAKE = 26201;
constexpr uint_fast16_t PORT_CONNECT = 26202;
constexpr uint_fast16_t PORT_COMMANDS = 26203;
constexpr uint_fast16_t PORT_CMD_TRANSMIT = 26204;
constexpr uint_fast16_t PORT_PERIODIC = 26207;
constexpr uint_fast16_t PORT_CALLBACKS = 26208;
constexpr uint_fast16_t PORT_CLOCKSYNC = 26209;
constexpr uint_fast16_t PORT_TLS = 26210;
constexpr uint_fast16_t PORT_TLS_OWNER = 26235;

} // namespace

// ===========================================================================
// 1. Server start()/stop() + run thread + real connection acceptance +
//    interrogation + end-of-initialization + onNewStation/onNewPoint firing.
// ===========================================================================
TEST_CASE("Loopback: handshake, interrogation, end of initialization",
          "[loopback][server][client]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_HANDSHAKE;
  const uint_fast16_t ca = 47;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  REQUIRE(server != nullptr);
  auto station = server->addStation(ca);
  REQUIRE(station != nullptr);
  // a few monitoring points so the interrogation round trip moves real data
  station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);
  station->addPoint(12, IEC60870_5_TypeID::M_SP_NA_1);

  REQUIRE_NOTHROW(server->start());
  REQUIRE(server->isRunning());

  auto client = Client::create(50, 10000);
  REQUIRE(client != nullptr);
  auto conn = client->addConnection("127.0.0.1", port, INIT_INTERROGATION);
  REQUIRE(conn != nullptr);
  // register the "new station / new point / initialized" callbacks
  withGlobal("_loop_cb_on_new_station",
             [&](py::object &cb) { client->setOnNewStationCallback(cb); });
  withGlobal("_loop_cb_on_new_point",
             [&](py::object &cb) { client->setOnNewPointCallback(cb); });
  withGlobal("_loop_cb_on_station_initialized", [&](py::object &cb) {
    client->setOnEndOfInitializationCallback(cb);
  });

  REQUIRE_NOTHROW(client->start());
  REQUIRE(client->isRunning());

  // ---- ASYNC handshake: poll until the connection is open ----
  bool opened = waitUntil([&]() { return conn->getState() == OPEN; }, 8000);
  REQUIRE(opened);

  // server should now see an active (open, unmuted) connection
  bool serverActive =
      waitUntil([&]() { return server->hasActiveConnections(); }, 8000);
  REQUIRE(serverActive);
  REQUIRE(server->getActiveConnectionCount() >= 1);
  REQUIRE(server->getOpenConnectionCount() >= 1);
  REQUIRE(server->hasOpenConnections());

  // ---- signal end of initialization on the server station; client's
  //      on_station_initialized fires through real bytes ----
  REQUIRE_NOTHROW(
      station->sendEndOfInitialization(static_cast<CS101_CauseOfInitialization>(
          IEC60870_COI_LOCAL_MANUAL_RESET)));

  // let the M_EI_NA_1 message round trip
  std::this_thread::sleep_for(500ms);

  // Wait for the auto-interrogation (INIT_INTERROGATION) to complete and the
  // state to transition from OPEN_MUTED to OPEN.  This must happen BEFORE any
  // manual interrogation: the auto-interrogation sends to GLOBAL_CA=65535, but
  // the server responds with each station's CA (47).  If a manual interrogation
  // registers "47-C_IC_NA_1-0" while the auto-interrogation is still in flight,
  // the auto's ACT_TERM (CA=47) would be stolen by the manual's cmdId match,
  // breaking both interrogations.
  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));

  // start an interrogation from the client over the live socket
  bool giOk = conn->interrogation(ca, CS101_COT_ACTIVATION, QOI_STATION, true);
  REQUIRE(giOk);

  // connection must still be fully OPEN (not muted) after manual interrogation
  REQUIRE(conn->getState() == OPEN);

  // teardown: always stop, must not throw
  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 2. Client connect()/isOpen()/disconnect() + reconnectAll()
// ===========================================================================
TEST_CASE("Loopback: connect, disconnect, reconnectAll", "[loopback][client]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_CONNECT;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  server->addStation(1);
  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  REQUIRE(conn != nullptr);
  REQUIRE(conn->isOpen() == false);
  REQUIRE(conn->getState() == CLOSED);

  REQUIRE_NOTHROW(client->start());

  // poll for open state (real TCP connect + START_DT)
  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));
  REQUIRE(conn->getState() == OPEN);
  REQUIRE(client->hasOpenConnections());
  REQUIRE(client->getOpenConnectionCount() >= 1);

  // getConnectedAt should now be set (requires open path)
  REQUIRE(conn->getConnectedAt().has_value());

  // controlled disconnect
  REQUIRE_NOTHROW(conn->disconnect());
  REQUIRE(waitUntil([&]() { return !conn->isOpen(); }, 8000));
  REQUIRE(conn->getDisconnectedAt().has_value());

  // reconnectAll: auto-reconnect
  REQUIRE_NOTHROW(client->reconnectAll());
  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));

  // mute/unmute on an open connection
  // mute() sends StopDT asynchronously; state transitions to OPEN_MUTED only
  // when STOPDT_CON arrives from the server.
  REQUIRE(conn->mute() == true);
  REQUIRE(waitUntil([&]() { return conn->isMuted(); }, 8000));
  REQUIRE(conn->unmute() == true);
  REQUIRE(waitUntil([&]() { return !conn->isMuted(); }, 8000));

  // disconnectAll via client teardown
  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 3. Client command success paths: clockSync(), test(), interrogation()
// ===========================================================================
TEST_CASE("Loopback: client clockSync, test, interrogation commands",
          "[loopback][client][command]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_COMMANDS;
  const uint_fast16_t ca = 3;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  auto station = server->addStation(ca);
  station->addPoint(101, IEC60870_5_TypeID::M_ME_NC_1);
  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  client->start();

  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));

  // clockSync success requires OPEN state + a server that replies
  bool cs = conn->clockSync(ca, true);
  REQUIRE(cs == true);

  // Test commands (C_TS_TA_1). The server responds positively only when it
  // owns a matching test-command point; without one the confirmation is
  // negative. Either way the client-side open-state gate, command
  // preparation, socket send and response-await path all execute — which is
  // what we are covering here — so we do not hard-require `true` (that would
  // be flaky depending on the server's point set).
  bool tc = conn->test(ca, true, true);
  bool tc2 = conn->test(ca, false, true);
  CAPTURE(tc, tc2);

  // a fresh interrogation over the open connection
  bool gi = conn->interrogation(ca, CS101_COT_ACTIVATION, QOI_STATION, true);
  REQUIRE(gi == true);

  // invalid qualifier must throw (bounds check in interrogation)
  REQUIRE_THROWS_AS(
      conn->interrogation(ca, CS101_COT_ACTIVATION,
                          static_cast<CS101_QualifierOfInterrogation>(0), true),
      std::invalid_argument);

  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 4. Command transmit from client -> server execute()/asduHandler + read()
// ===========================================================================
TEST_CASE("Loopback: client command transmit + read round trips",
          "[loopback][client][command][server]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_CMD_TRANSMIT;
  const uint_fast16_t ca = 5;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  auto station = server->addStation(ca);
  // server-side single command point (C_SC_NA_1)
  auto svCmd = station->addPoint(201, IEC60870_5_TypeID::C_SC_NA_1);
  REQUIRE(svCmd != nullptr);
  // server-side monitoring point for read
  auto svMeas = station->addPoint(202, IEC60870_5_TypeID::M_ME_NC_1);
  REQUIRE(svMeas != nullptr);
  svMeas->setValue(InfoValue{42.0f});
  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  auto clStation = conn->addStation(ca);
  REQUIRE(clStation != nullptr);
  auto clCmd = clStation->addPoint(201, IEC60870_5_TypeID::C_SC_NA_1);
  REQUIRE(clCmd != nullptr);
  auto clMeas = clStation->addPoint(202, IEC60870_5_TypeID::M_ME_NC_1);
  REQUIRE(clMeas != nullptr);
  client->start();

  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));

  // client -> server single command (C_SC_NA_1) transmit; server must respond
  // ACT_CON (and ACT_TERM). Direct command returns on COMMAND_AWAIT_CON.
  clCmd->setValue(InfoValue{true});
  bool cmdOk = conn->transmit(clCmd, CS101_COT_ACTIVATION);
  REQUIRE(cmdOk == true);

  // client read of a monitoring point -> server readHandler serves the value
  bool readOk = conn->read(clMeas, true);
  REQUIRE(readOk == true);

  // the client point should now have received the server's value (42.0)
  auto v = clMeas->getValue();
  bool gotValue =
      std::holds_alternative<float>(v) && std::get<float>(v) == 42.0f;
  REQUIRE(gotValue);

  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 5. Server spontaneous transmit + periodic inventory (sendPeriodicInventory)
// ===========================================================================
TEST_CASE("Loopback: server spontaneous + periodic transmit to real client",
          "[loopback][server][client][periodic]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_PERIODIC;
  const uint_fast16_t ca = 7;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  auto station = server->addStation(ca);

  // reporting point with report_ms -> drives sendPeriodicInventory
  auto svPeriodic = station->addPoint(301, IEC60870_5_TypeID::M_ME_NC_1, 300);
  REQUIRE(svPeriodic != nullptr);
  svPeriodic->setValue(InfoValue{1.0f});

  // point used for a manual spontaneous transmit
  auto svSpont = station->addPoint(302, IEC60870_5_TypeID::M_ME_NC_1);
  REQUIRE(svSpont != nullptr);
  svSpont->setValue(InfoValue{2.0f});

  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  auto clStation = conn->addStation(ca);
  auto clPeriodic = clStation->addPoint(301, IEC60870_5_TypeID::M_ME_NC_1);
  auto clSpont = clStation->addPoint(302, IEC60870_5_TypeID::M_ME_NC_1);
  withGlobal("_loop_cb_pt_receive",
             [&](py::object &cb) { clPeriodic->setOnReceiveCallback(cb); });
  withGlobal("_loop_cb_pt_receive",
             [&](py::object &cb) { clSpont->setOnReceiveCallback(cb); });
  client->start();

  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));
  // Ensure the server has registered the connection as active (STARTDT
  // received) before testing periodic/spontaneous delivery, otherwise
  // sendPeriodicInventory may skip all stations on its first ticks.
  REQUIRE(waitUntil([&]() { return server->hasActiveConnections(); }, 8000));

  // spontaneous transmit: server enqueues a SPONTANEOUS (low priority) ASDU,
  // the slave floods it to the connected client -> client on_receive fires and
  // the client point value updates.
  svSpont->setValue(InfoValue{7.5f});
  REQUIRE_NOTHROW(svSpont->transmit(CS101_COT_SPONTANEOUS));

  bool spontReceived = waitUntil(
      [&]() {
        auto v = clSpont->getValue();
        return std::holds_alternative<float>(v) && std::get<float>(v) == 7.5f;
      },
      8000);
  REQUIRE(spontReceived);

  // periodic inventory: the report_ms point auto-transmits every ~300ms; the
  // connected client should observe the updated value.
  svPeriodic->setValue(InfoValue{9.0f});
  // sendPeriodicInventory is scheduled every tickRate (50ms); nextReportAt
  // gates emission every 300ms. Poll for the client-side value update.
  bool periodicReceived = waitUntil(
      [&]() {
        auto v = clPeriodic->getValue();
        return std::holds_alternative<float>(v) && std::get<float>(v) == 9.0f;
      },
      8000);
  REQUIRE(periodicReceived);

  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 6. Connection onStateChange + raw message handlers firing on real bytes +
//    server on_connect accept / reject.
// ===========================================================================
TEST_CASE("Loopback: state-change + raw handlers + on_connect accept/reject",
          "[loopback][connection][callback][server]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_CALLBACKS;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  server->addStation(9);
  withGlobal("_loop_cb_on_connect_accept",
             [&](py::object &cb) { server->setOnConnectCallback(cb); });
  withGlobal("_loop_cb_recv_raw_server",
             [&](py::object &cb) { server->setOnReceiveRawCallback(cb); });
  withGlobal("_loop_cb_send_raw_server",
             [&](py::object &cb) { server->setOnSendRawCallback(cb); });
  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  withGlobal("_loop_cb_state_change",
             [&](py::object &cb) { conn->setOnStateChangeCallback(cb); });
  withGlobal("_loop_cb_recv_raw_con",
             [&](py::object &cb) { conn->setOnReceiveRawCallback(cb); });
  withGlobal("_loop_cb_send_raw_con",
             [&](py::object &cb) { conn->setOnSendRawCallback(cb); });
  client->start();

  // accepted connection: on_connect returns True.
  // hasActiveConnections() is incremented when the server receives STARTDT,
  // which happens asynchronously after the client's isOpen() turns true.
  bool opened = waitUntil([&]() { return conn->getState() == OPEN; }, 8000);
  REQUIRE(opened);
  REQUIRE(waitUntil([&]() { return server->hasActiveConnections(); }, 8000));

  // drive some traffic so raw handlers fire on real bytes both directions
  bool cs = conn->clockSync(9, true);
  REQUIRE(cs == true);

  // disconnect -> on_state_change should move through real transitions
  REQUIRE_NOTHROW(conn->disconnect());
  REQUIRE(waitUntil([&]() { return !conn->isOpen(); }, 8000));

  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());

  // ---- reject path: a server that refuses connections (on_connect -> False).
  //      Use a dedicated port so there is no overlap with other TEST_CASEs.
  const uint_fast16_t rport = 26219;
  auto rejectServer = Server::create("127.0.0.1", rport, 50, 10000, 8);
  rejectServer->addStation(10);
  withGlobal("_loop_cb_on_connect_reject",
             [&](py::object &cb) { rejectServer->setOnConnectCallback(cb); });
  REQUIRE_NOTHROW(rejectServer->start());

  auto rejectClient = Client::create(50, 10000);
  auto rconn = rejectClient->addConnection("127.0.0.1", rport, INIT_NONE);
  rejectClient->start();

  // server rejects; connection may briefly reach OPEN_MUTED during TCP
  // handshake, but the server immediately closes the socket, causing the
  // client to transition to CLOSED_AWAIT_RECONNECT (proving rejection).
  REQUIRE(waitUntil(
      [&]() {
        return rconn->getState() == ConnectionState::CLOSED_AWAIT_RECONNECT;
      },
      3000));

  REQUIRE_NOTHROW(rejectClient->stop());
  REQUIRE_NOTHROW(rejectServer->stop());
}

// ===========================================================================
// 7. Server clock sync handler firing on a real clock-sync command.
// ===========================================================================
TEST_CASE("Loopback: server on_clock_sync via client clockSync",
          "[loopback][server][clock-sync]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_CLOCKSYNC;
  const uint_fast16_t ca = 12;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  server->addStation(ca);
  withGlobal("_loop_cb_on_clock_sync",
             [&](py::object &cb) { server->setOnClockSyncCallback(cb); });
  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  client->start();

  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));
  bool cs = conn->clockSync(ca, true);
  REQUIRE(cs == true);

  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 8. TLS loopback handshake (feasible when lib60870 is built with mbedTLS).
//    The mbedTLS source is copied into lib60870 by CMakeLists, so the secure
//    create paths compile; a handshake requires valid certs. Guarded so a
//    TLS-unavailable/unsupported build never destabilizes CI.
// ===========================================================================
TEST_CASE("Loopback: TLS handshake over loopback socket",
          "[loopback][tls][!mayfail]") {
  ensurePythonCallbacks();
  const uint_fast16_t port = PORT_TLS;
  const uint_fast16_t ca = 14;

  auto serverCert = resolveCertPath("server1.crt");
  auto serverKey = resolveCertPath("server1.key");
  auto caCert = resolveCertPath("ca.crt");
  auto clientCert = resolveCertPath("client1.crt");
  auto clientKey = resolveCertPath("client1.key");

  // server TLS config: present our cert/key, trust the CA
  auto serverTs = Remote::TransportSecurity::create(false, false);
  REQUIRE(serverTs != nullptr);
  REQUIRE_NOTHROW(serverTs->setCertificate(serverCert, serverKey));
  REQUIRE_NOTHROW(serverTs->setCACertificate(caCert));
  auto server = Server::create("127.0.0.1", port, 50, 10000, 8, serverTs);
  REQUIRE(server != nullptr);
  server->addStation(ca);

  // client TLS config: trust the CA, present the client cert
  auto clientTs = Remote::TransportSecurity::create(false, false);
  REQUIRE(clientTs != nullptr);
  REQUIRE_NOTHROW(clientTs->setCertificate(clientCert, clientKey));
  REQUIRE_NOTHROW(clientTs->setCACertificate(caCert));

  REQUIRE_NOTHROW(server->start());
  REQUIRE(server->isRunning());

  auto client = Client::create(50, 10000, clientTs);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  REQUIRE(conn != nullptr);
  client->start();

  // TLS handshake is async; poll with a generous timeout. If the local build
  // does not support TLS the slave may fail to accept -> connection never
  // opens, guarded by [!mayfail].
  bool opened = waitUntil([&]() { return conn->getState() == OPEN; }, 8000);
  if (opened) {
    // push some traffic over the encrypted channel
    bool cs = conn->clockSync(ca, true);
    REQUIRE(cs == true);
    bool gi = conn->interrogation(ca, CS101_COT_ACTIVATION, QOI_STATION, true);
    REQUIRE(gi == true);
  }

  REQUIRE_NOTHROW(client->stop());
  REQUIRE_NOTHROW(server->stop());
}

// ===========================================================================
// 9. SEC-11 / M14: the TLS event handler drops the last Server reference.
//    TLS_SEC_EVT_INFO is raised inline by mbedtls_ssl_handshake() inside
//    TLSSocket_create() from MasterConnection_init(), which the slave runs on
//    its own accepting thread. That thread carries no Server reference, so the
//    handler that drops the last one destroys the Server on it: ~Server() must
//    recognise the thread (ScopedThreadOwner::ANY) and hand CS104_Slave_stop()
//    to a neutral thread. Otherwise the stop self-waits on the calling thread
//    and the TLS configuration can be torn down mid-handshake.
// ===========================================================================
TEST_CASE("Loopback: TLS on_event dropping the last Server reference defers "
          "teardown",
          "[loopback][tls][thread-owner][security]") {
  const uint_fast16_t port = PORT_TLS_OWNER;
  const uint_fast16_t ca = 15;

  auto serverCert = resolveCertPath("server1.crt");
  auto serverKey = resolveCertPath("server1.key");
  auto caCert = resolveCertPath("ca.crt");
  auto clientCert = resolveCertPath("client1.crt");
  auto clientKey = resolveCertPath("client1.key");
  if (!fileExists(serverCert) || !fileExists(serverKey) ||
      !fileExists(caCert) || !fileExists(clientCert) || !fileExists(clientKey))
    SKIP("TLS test certificates not available");

  py::exec(R"(
import c104
import threading

_tls_owner_inside = threading.Event()
_tls_owner_release = threading.Event()
_tls_owner_server = None
_tls_owner_events = []

def _tls_owner_reset() -> None:
    _tls_owner_inside.clear()
    _tls_owner_release.clear()
    del _tls_owner_events[:]
    global _tls_owner_server
    _tls_owner_server = None

def _tls_owner_on_event(tls: c104.TransportSecurity, level: c104.TlsEventLevel, code: int, message: str, peer: str) -> None:
    global _tls_owner_server
    _tls_owner_events.append((int(level), code))
    # Park on the first event: it is raised on the slave's accepting thread,
    # the very thread CS104_Slave_stop() destroys, so dropping the reference
    # here is what forces ~Server() to recognise that thread.
    _tls_owner_inside.set()
    _tls_owner_release.wait(10.0)
    # the handler, not the test thread, drops the last reference: the teardown
    # then starts on the thread the handler runs on
    _tls_owner_server = None
)",
           py::globals());
  py::globals()["_tls_owner_reset"]();

  auto serverTs = Remote::TransportSecurity::create(false, false);
  REQUIRE(serverTs != nullptr);
  REQUIRE_NOTHROW(serverTs->setCertificate(serverCert, serverKey));
  REQUIRE_NOTHROW(serverTs->setCACertificate(caCert));
  {
    py::object cb = py::globals()["_tls_owner_on_event"];
    REQUIRE_NOTHROW(serverTs->setOnEventCallback(cb));
  }

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8, serverTs);
  REQUIRE(server != nullptr);
  server->addStation(ca);
  REQUIRE_NOTHROW(server->start());
  REQUIRE(server->isRunning());
  void *const key = server->getCallbackKey();

  // hand the only strong reference to python before any client can connect
  py::globals()["_tls_owner_server"] = py::cast(server);
  server.reset();

  auto clientTs = Remote::TransportSecurity::create(false, false);
  REQUIRE(clientTs != nullptr);
  REQUIRE_NOTHROW(clientTs->setCertificate(clientCert, clientKey));
  REQUIRE_NOTHROW(clientTs->setCACertificate(caCert));

  auto client = Client::create(50, 10000, clientTs);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  REQUIRE(conn != nullptr);
  client->start();

  // the server side handler parks inside the TLS handshake
  REQUIRE(py::cast<bool>(py::globals()["_tls_owner_inside"].attr("wait")(8.0)));
  // it fired while the handshake was running, the instance is still registered
  REQUIRE(Server::getInstance(key) != nullptr);

  // let the handler return and drop the last reference on the accepting thread
  py::globals()["_tls_owner_release"].attr("set")();

#if defined(Py_GIL_DISABLED)
  // Free-threaded CPython defers the dealloc of an object whose last
  // reference was dropped by a thread that is not its owning thread (the
  // accepting thread here). The dealloc runs only when the owning thread
  // executes Python again, so pump the GC from this thread to observe the
  // deferred teardown within the deadline.
  REQUIRE(waitUntil(
      [&]() {
        {
          py::gil_scoped_acquire const gil;
          py::module_::import("gc").attr("collect")();
        }
        return Server::getInstance(key) == nullptr;
      },
      8000));
#else
  REQUIRE(
      waitUntil([&]() { return Server::getInstance(key) == nullptr; }, 8000));
#endif

  // The slave kept running after the handler dropped the last reference: the
  // deferred teardown must not tear the accepting thread down underneath the
  // handshake still in flight. A destructor that self-joined the accepting
  // thread instead deadlocks inside CS104_Slave_stop(), the loop stops
  // accepting, and the session never opens.
  REQUIRE(waitUntil([&]() { return conn->getState() == OPEN; }, 8000));

  REQUIRE_NOTHROW(client->stop());
  conn.reset();
  client.reset();

  // the listening socket is only released once the deferred teardown completed
  REQUIRE(waitUntil([&]() { return portIsFree(port); }, 8000, 100));

  // the handler really saw a TLS security event, not just a closed connection
  py::list const events = py::globals()["_tls_owner_events"];
  REQUIRE(py::len(events) > 0);
  bool sawEstablished = false;
  for (auto const &event : events) {
    py::tuple const pair = py::reinterpret_borrow<py::tuple>(event);
    if (py::cast<int>(pair[1]) == TLS_EVENT_CODE_INF_SESSION_ESTABLISHED)
      sawEstablished = true;
  }
  REQUIRE(sawEstablished);
}
