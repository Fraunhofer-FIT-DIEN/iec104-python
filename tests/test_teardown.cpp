/**
 * Copyright 2020-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file test_teardown.cpp
 * @brief Regression tests for SECURITY.md M1 (GilAwareMutex::lock blocks
 * instead of throwing inside lib60870 callbacks) and M2 (destruction of a
 * Server, Client or Connection on a lib60870 callback thread or on its own
 * run thread is handed to a neutral thread instead of self-joining).
 *
 * The lifecycle cases use real loopback sockets: a python callback blocks the
 * lib60870 / run thread inside the handler while the test drops its last
 * reference, so the handler's shared_ptr becomes the last owner.
 *
 * @package iec104-python
 * @namespace tests
 */

#include <atomic>
#include <chrono>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include "Client.h"
#include "Server.h"
#include "module/GilAwareMutex.h"
#include "module/GilState.h"
#include "module/ThreadOwner.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/Connection.h"
#include "types.h"

namespace py = pybind11;
using namespace std::chrono_literals;

namespace {

/// Poll @p fn with the GIL released, so lib60870 and run threads can fire
/// python callbacks while the test waits.
template <typename Fn>
bool waitUntil(Fn &&fn, int timeout_ms, int step_ms = 20) {
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

/// Python callbacks that park the calling thread inside the handler until the
/// test releases them (tests/main.cpp holds the process-lifetime interpreter).
void ensureTeardownCallbacks() {
  static bool inited = false;
  if (inited)
    return;
  inited = true;
  py::exec(R"(
import threading
import c104

_td_inside = threading.Event()
_td_release = threading.Event()

def _td_reset() -> None:
    _td_inside.clear()
    _td_release.clear()

def _td_on_connect(server: c104.Server, ip: str) -> bool:
    _td_inside.set()
    _td_release.wait(10.0)
    return True

def _td_on_receive_raw(server: c104.Server, data: bytes) -> None:
    _td_inside.set()
    _td_release.wait(10.0)

def _td_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
    _td_inside.set()
    _td_release.wait(10.0)
)",
           py::globals());
}

py::object global(const char *name) { return py::globals()[name]; }

void resetEvents() { global("_td_reset")(); }

/// Wait until a callback signalled that it runs (Event.wait drops the GIL).
bool waitInside(int timeout_ms) {
  return py::cast<bool>(global("_td_inside").attr("wait")(timeout_ms / 1000.0));
}

void releaseCallback() { global("_td_release").attr("set")(); }

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

constexpr uint_fast16_t PORT_SERVER_LISTENING_THREAD = 26231;
constexpr uint_fast16_t PORT_SERVER_RUN_THREAD = 26232;
constexpr uint_fast16_t PORT_CLIENT_RECEIVE_THREAD = 26233;

} // namespace

// ===========================================================================
// M1: GilAwareMutex
// ===========================================================================

TEST_CASE("GilAwareMutex::lock blocks instead of throwing after 100 ms",
          "[module][mutex][security]") {
  Module::GilAwareMutex mutex("test_mutex");
  std::atomic_bool locked{false};

  std::thread holder([&]() {
    mutex.lock();
    locked.store(true);
    std::this_thread::sleep_for(350ms);
    mutex.unlock();
  });
  while (!locked.load())
    std::this_thread::yield();

  REQUIRE(mutex.try_lock() == false);

  // before the fix this threw std::runtime_error("Potential Deadlock ...")
  // after 100 ms, which terminates the process when it happens inside a
  // noexcept Callback member or a lib60870 C callback
  auto const begin = std::chrono::steady_clock::now();
  REQUIRE_NOTHROW(mutex.lock());
  auto const waited = std::chrono::steady_clock::now() - begin;
  mutex.unlock();
  holder.join();

  REQUIRE(waited >= 200ms);
  // the GIL held by the test thread is restored after the blocking wait
  REQUIRE(Module::hasAttachedThreadState());
}

TEST_CASE("GilAwareMutex works through std::lock_guard in noexcept context",
          "[module][mutex][security]") {
  Module::GilAwareMutex mutex("test_mutex_guard");
  auto const locker = [&]() noexcept {
    std::lock_guard<Module::GilAwareMutex> const lock(mutex);
    return true;
  };
  std::thread holder([&]() {
    std::lock_guard<Module::GilAwareMutex> const lock(mutex);
    std::this_thread::sleep_for(250ms);
  });
  std::this_thread::sleep_for(50ms);
  REQUIRE(locker());
  holder.join();
}

// ===========================================================================
// M2: ScopedThreadOwner
// ===========================================================================

TEST_CASE("ScopedThreadOwner marks the current thread only",
          "[module][thread-owner][security]") {
  int a = 0;
  int b = 0;
  REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
  REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(nullptr));
  {
    Module::ScopedThreadOwner const outer(&a);
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
    REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));
    {
      Module::ScopedThreadOwner const inner(&b);
      REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
      REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));

      // markers are thread local
      bool seenElsewhere = true;
      std::thread([&]() {
        seenElsewhere = Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a) ||
                        Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b);
      }).join();
      REQUIRE_FALSE(seenElsewhere);

      REQUIRE_FALSE(outer.ownerDestroyed());
      Module::ScopedThreadOwner::markDestroyed(&a);
      REQUIRE(outer.ownerDestroyed());
      REQUIRE_FALSE(inner.ownerDestroyed());
    }
    REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
  }
  REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
}

TEST_CASE("ScopedThreadOwner::ANY matches every key, nests and restores",
          "[module][thread-owner][security]") {
  int a = 0;
  int b = 0;
  REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));

  // the wildcard protects an object whose key the thread cannot name, e.g. a
  // lib60870 TLS event handler running on the handshake thread
  {
    Module::ScopedThreadOwner const wildcard(Module::ScopedThreadOwner::ANY);
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(
        static_cast<const void *>("unrelated key")));
    // a wildcard never makes "no owner" match
    REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(nullptr));
    // it is not a run loop of a single object, so it is never destroyed
    Module::ScopedThreadOwner::markDestroyed(&a);
    REQUIRE_FALSE(wildcard.ownerDestroyed());

    // a keyed marker nests inside the wildcard; ordinary key semantics stay
    // untouched: the keyed marker still only matches its own key
    {
      Module::ScopedThreadOwner const inner(&b);
      REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
      REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));
      REQUIRE(inner.ownerDestroyed() == false);
      Module::ScopedThreadOwner::markDestroyed(&b);
      REQUIRE(inner.ownerDestroyed());
      REQUIRE_FALSE(wildcard.ownerDestroyed());

      // markers stay thread local
      bool seenElsewhere = true;
      std::thread([&]() {
        seenElsewhere = Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a);
      }).join();
      REQUIRE_FALSE(seenElsewhere);
    }
    // the keyed marker is gone again, so the wildcard alone governs this
    // thread and every key matches once more. That the keyed marker stack
    // itself is restored is covered by the keyed-only case above; the
    // wildcard must not change those ordinary key semantics.
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
  }
  // the wildcard marker is strictly scoped
  REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));

  // a keyed marker below a removed wildcard still works as before
  {
    Module::ScopedThreadOwner const keyed(&a);
    REQUIRE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
    REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&b));
  }
  REQUIRE_FALSE(Module::ScopedThreadOwner::isCurrentThreadOwnedBy(&a));
}

TEST_CASE("nextCallbackKey is unique and distinct from object addresses",
          "[module][thread-owner][security]") {
  auto server = Server::create("127.0.0.1", 26230);
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 26230);

  REQUIRE(server->getCallbackKey() != nullptr);
  REQUIRE(conn->getCallbackKey() != nullptr);
  REQUIRE(server->getCallbackKey() != conn->getCallbackKey());
  REQUIRE(server->getCallbackKey() != static_cast<void *>(server.get()));
  REQUIRE(conn->getCallbackKey() != static_cast<void *>(conn.get()));
  REQUIRE(Module::nextCallbackKey() != Module::nextCallbackKey());

  // the key, not the address, resolves the instance
  REQUIRE(Server::getInstance(server->getCallbackKey()) == server);
  REQUIRE(Server::getInstance(static_cast<void *>(server.get())) == nullptr);
  REQUIRE(Remote::Connection::getInstance(conn->getCallbackKey()) == conn);
}

// ===========================================================================
// M2: destruction on a thread the teardown would join
// ===========================================================================

TEST_CASE("Server: last reference dropped inside on_connect (listening thread)",
          "[server][lifecycle][security]") {
  ensureTeardownCallbacks();
  resetEvents();
  const uint_fast16_t port = PORT_SERVER_LISTENING_THREAD;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  {
    py::object cb = global("_td_on_connect");
    server->setOnConnectCallback(cb);
  }
  REQUIRE_NOTHROW(server->start());
  void *const key = server->getCallbackKey();
  REQUIRE(Server::getInstance(key) != nullptr);

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  client->start();

  // the lib60870 listening thread is parked inside on_connect and holds the
  // instance via connectionRequestHandler()
  REQUIRE(waitInside(8000));

  // drop the test's reference: the handler's shared_ptr and the python wrapper
  // handed to the callback are the only owners left
  server.reset();
  REQUIRE(Server::getInstance(key) != nullptr);
  releaseCallback();

  // on_connect returns, the handler drops the last owner and ~Server() runs on
  // the listening thread. Before the fix CS104_Slave_stop() waited forever for
  // its own thread to finish (or, on a connection thread, joined itself and
  // freed the stack it was running on).
  REQUIRE(
      waitUntil([&]() { return Server::getInstance(key) == nullptr; }, 8000));
  // the deferred teardown really destroyed the slave: the port is free again
  REQUIRE(waitUntil([&]() { return portIsFree(port); }, 8000, 100));
  REQUIRE(waitUntil([&]() { return !conn->isOpen(); }, 8000));

  REQUIRE_NOTHROW(client->stop());
}

TEST_CASE("Server: last reference dropped inside on_receive_raw (run thread)",
          "[server][lifecycle][security]") {
  ensureTeardownCallbacks();
  resetEvents();
  const uint_fast16_t port = PORT_SERVER_RUN_THREAD;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  {
    py::object cb = global("_td_on_receive_raw");
    server->setOnReceiveRawCallback(cb);
  }
  REQUIRE_NOTHROW(server->start());
  void *const key = server->getCallbackKey();

  auto client = Client::create(50, 10000);
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  client->start();

  // the client's STARTDT frame is reported through rawMessageHandler, which
  // schedules on_receive_raw on the server's run thread; that task holds the
  // instance and is now parked inside the callback
  REQUIRE(waitInside(8000));

  server.reset();
  releaseCallback();

  // the task's shared_ptr is the last owner: ~Server() runs on the run thread.
  // Before the fix stop() joined the current thread, std::system_error from a
  // destructor -> std::terminate.
  REQUIRE(
      waitUntil([&]() { return Server::getInstance(key) == nullptr; }, 8000));
  REQUIRE(waitUntil([&]() { return portIsFree(port); }, 8000, 100));

  REQUIRE_NOTHROW(client->stop());
}

TEST_CASE("Client: last reference dropped inside on_new_station (receive "
          "thread)",
          "[client][connection][lifecycle][security]") {
  ensureTeardownCallbacks();
  resetEvents();
  const uint_fast16_t port = PORT_CLIENT_RECEIVE_THREAD;
  const uint_fast16_t ca = 5;

  auto server = Server::create("127.0.0.1", port, 50, 10000, 8);
  auto station = server->addStation(ca);
  // periodic report: the first frame reaches a client without that station
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1, 200);
  REQUIRE(point != nullptr);
  point->setValue(InfoValue{1.0f});
  REQUIRE_NOTHROW(server->start());

  auto client = Client::create(50, 10000);
  {
    py::object cb = global("_td_on_new_station");
    client->setOnNewStationCallback(cb);
  }
  auto conn = client->addConnection("127.0.0.1", port, INIT_NONE);
  void *const connKey = conn->getCallbackKey();
  client->start();

  // the connection's receive thread is parked inside on_new_station, called
  // synchronously from Connection::asduHandler, holding client and connection
  REQUIRE(waitInside(8000));

  conn.reset();
  client.reset();
  releaseCallback();

  // the handler's locals are the last owners: ~Client() and ~Connection() run
  // on the receive thread. Before the fix ~Client() -> disconnectAll() joined
  // the current thread and ~Connection() -> CS104_Connection_destroy() freed
  // the connection struct the thread was still executing on.
  REQUIRE(waitUntil(
      [&]() { return Remote::Connection::getInstance(connKey) == nullptr; },
      8000));
  // the deferred close reached the server
  REQUIRE(waitUntil([&]() { return !server->hasOpenConnections(); }, 8000));

  REQUIRE_NOTHROW(server->stop());
}

TEST_CASE("Connection: destructor on an owned thread defers the lib60870 "
          "destroy",
          "[connection][lifecycle][security]") {
  auto client = Client::create();
  auto conn = client->addConnection("127.0.0.1", 26234);
  void *const key = conn->getCallbackKey();
  REQUIRE(client->removeConnection("127.0.0.1", 26234));

  // emulate the receive thread dropping the last reference inside a handler;
  // the GIL is released because ~Connection() acquires it for its callbacks
  {
    py::gil_scoped_release const gil_release;
    std::thread([&]() {
      Module::ScopedThreadOwner const owner(key);
      conn.reset();
    }).join();
    // give the neutral thread time to run CS104_Connection_destroy
    std::this_thread::sleep_for(100ms);
  }

  REQUIRE(Remote::Connection::getInstance(key) == nullptr);
}
