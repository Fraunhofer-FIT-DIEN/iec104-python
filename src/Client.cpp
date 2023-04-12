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
 *
 * @file Client.cpp
 * @brief operate a scada unit
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Client.h"

#include "module/ScopedGilAcquire.h"
#include "module/ScopedGilRelease.h"
#include "remote/Helper.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/OutgoingMessage.h"

using namespace Remote;
using namespace std::chrono_literals;

Client::Client(const std::uint_fast32_t tick_rate_ms,
               const std::uint_fast32_t timeout_ms,
               std::shared_ptr<Remote::TransportSecurity> transport_security)
    : tickRate_ms(tick_rate_ms), commandTimeout_ms(timeout_ms),
      security(std::move(transport_security)) {
  DEBUG_PRINT(Debug::Client, "Created");
}

Client::~Client() {
  // stops and destroys the slave
  stop();

  {
    std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
    connections.clear();
  }
  DEBUG_PRINT(Debug::Client, "Removed");
}

void Client::start() {
  bool expected = false;
  if (!enabled.compare_exchange_strong(expected, true)) {
    DEBUG_PRINT(Debug::Client, "start] Already running");
    return;
  }

  Module::ScopedGilRelease const scoped("Client.start");

  if (!runThread) {
    runThread = new std::thread(&Client::thread_run, this);
  }

  {
    std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
    for (auto &c : connections) {
      c->connect();
    }
  }

  DEBUG_PRINT(Debug::Client, "start] Started");
}

void Client::stop() {
  // stop all connections
  disconnectAll();

  bool expected = true;
  if (!enabled.compare_exchange_strong(expected, false)) {
    DEBUG_PRINT(Debug::Client, "stop] Already stopped");
    return;
  }

  if (running.load()) {
    runThread_wait.notify_all();
  }

  if (runThread) {
    runThread->join();
    delete runThread;
    runThread = nullptr;
  }

  DEBUG_PRINT(Debug::Client, "stop] Stopped");
}

bool Client::isRunning() { return enabled; }

void Client::setOriginatorAddress(uint_fast8_t address) {
  std::uint_fast8_t const prev = originatorAddress.load();
  if (prev != address) {
    originatorAddress.store(address);
    for (auto &c : connections) {
      if (c->getOriginatorAddress() == prev) {
        c->setOriginatorAddress(address);
      }
    }
    DEBUG_PRINT(Debug::Client,
                "set_originator_address] prev: " + std::to_string(prev) +
                    " | new: " + std::to_string(address));
  }
}

std::uint_fast8_t Client::getOriginatorAddress() const {
  return originatorAddress.load();
}

bool Client::hasConnections() {
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);

  return !connections.empty();
}

Remote::ConnectionVector Client::getConnections() {
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);

  return connections;
}

bool Client::hasConnection(const std::string &ip,
                           const std::uint_fast16_t port) {
  return nullptr != getConnection(ip, port).get();
}

std::shared_ptr<Remote::Connection>
Client::getConnection(const std::string &ip, const std::uint_fast16_t port) {
  std::string const conStr = Remote::connectionStringFormatter(ip, port);
  return getConnectionFromString(conStr);
}

std::shared_ptr<Remote::Connection>
Client::getConnectionFromString(const std::string &connectionString) {
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (auto &c : connections) {
    if (c->getConnectionString() == connectionString) {
      return c;
    }
  }

  return {nullptr};
}

std::shared_ptr<Remote::Connection>
Client::getConnectionFromCommonAddress(const std::uint_fast16_t commonAddress) {
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (auto &c : connections) {
    if (c->hasStation(commonAddress)) {
      return c;
    }
  }

  return {nullptr};
}

std::shared_ptr<Remote::Connection>
Client::addConnection(const std::string &ip, const uint_fast16_t port,
                      const ConnectionInit init) {
  if (hasConnection(ip, port)) {
    std::cerr << "[c104.Client.add_connection] Connection already exists"
              << std::endl;
    return {nullptr};
  }

  DEBUG_PRINT(Debug::Client,
              "add_connection] IP " + ip + " | PORT " + std::to_string(port));

  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  auto connection = Remote::Connection::create(
      shared_from_this(), ip, port, commandTimeout_ms, init, security,
      originatorAddress.load());
  connections.push_back(connection);

  return connection;
}

void Client::reconnectAll() {
  Module::ScopedGilRelease const scoped("Client.reconnectAll");

  std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
  for (auto &c : connections) {
    c->disconnect();
    c->connect();
  }
}

void Client::disconnectAll() {
  Module::ScopedGilRelease const scoped("Client.disconnectAll");

  std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
  for (auto &c : connections) {
    c->disconnect();
  }
}

void Client::setOnNewStationCallback(py::object &callable) {
  py_onNewStation.reset(callable);
}

void Client::onNewStation(std::shared_ptr<Remote::Connection> connection,
                          std::uint_fast16_t common_address) {
  if (py_onNewStation.is_set()) {
    DEBUG_PRINT(Debug::Client, "CALLBACK on_new_station");
    Module::ScopedGilAcquire const scoped("Client.on_new_station");
    py_onNewStation.call(shared_from_this(), std::move(connection),
                         common_address);
  } else {
    DEBUG_PRINT(Debug::Client,
                "CALLBACK on_new_station (default: add station)");
    // default behaviour
    connection->addStation(common_address);
  }
}

void Client::setOnNewPointCallback(py::object &callable) {
  py_onNewPoint.reset(callable);
}

void Client::onNewPoint(std::shared_ptr<Object::Station> station,
                        std::uint_fast32_t io_address, IEC60870_5_TypeID type) {

  if (py_onNewPoint.is_set()) {
    DEBUG_PRINT(Debug::Client, "CALLBACK on_new_point");
    Module::ScopedGilAcquire const scoped("Client.on_new_point");
    py_onNewPoint.call(shared_from_this(), std::move(station), io_address,
                       type);
  } else {
    DEBUG_PRINT(Debug::Client, "CALLBACK on_new_point (default: add point)");
    // default behaviour
    try {
      station->addPoint(io_address, type, 0, 0);
    } catch (const std::exception &e) {
      DEBUG_PRINT(Debug::Client, "on_new_point] Failed to add point: " +
                                     std::string(e.what()));
    }
  }
}

void Client::thread_run() {
  running.store(true);
  std::unique_lock<std::mutex> lock(runThread_mutex);
  std::chrono::system_clock::time_point desiredEnd;
  bool debug = false;
  uint_fast8_t count = 0;
  uint_fast8_t active = 0;

  while (enabled.load()) {
    debug = DEBUG_TEST(Debug::Client);
    desiredEnd = std::chrono::system_clock::now() + tickRate_ms.load() * 1ms;

    count = 0;
    active = 0;
    std::unique_lock<Module::GilAwareMutex> con_lock(connections_mutex);
    for (auto &c : connections) {
      ConnectionState const s = c->getState();
      switch (s) {
      case OPEN_MUTED:
      case OPEN:
        count++;
        if (!c->isMuted()) {
          active++;
        }
        break;
      case OPEN_AWAIT_INTERROGATION:
        count++;
        try {
          c->interrogation(IEC60870_GLOBAL_COMMON_ADDRESS, CS101_COT_ACTIVATION,
                           QOI_STATION, false);
        } catch (const std::exception &e) {
          std::cerr << "[c104.Client.loop] Failed to send connection "
                       "initiation interrogation: "
                    << e.what() << std::endl;
        }
        break;
      case OPEN_AWAIT_CLOCK_SYNC:
        count++;
        c->clockSync(IEC60870_GLOBAL_COMMON_ADDRESS, false);
        break;
      case CLOSED_AWAIT_RECONNECT:
        c->connect();
        break;
      }
    }
    con_lock.unlock();

    if (count != openConnections.load()) {
      openConnections.store(count);

      DEBUG_PRINT_CONDITION(debug, Debug::Client,
                            "thread_run] Connected servers: " +
                                std::to_string(count));
    }

    if (active != activeConnections.load()) {
      activeConnections.store(active);

      DEBUG_PRINT_CONDITION(debug, Debug::Client,
                            "thread_run] Active servers: " +
                                std::to_string(count));
    }

    runThread_wait.wait_until(lock, desiredEnd);
    if (debug) {
      auto diff = std::chrono::duration_cast<std::chrono::microseconds>(
                      std::chrono::system_clock::now() - desiredEnd)
                      .count();
      if (diff > 5000) {
        DEBUG_PRINT_CONDITION(true, Debug::Client,
                              "thread_run] Cannot keep up the tick rate: " +
                                  std::to_string(diff) + u8" \xb5s");
      }
    }
  }

  running.store(false);
}
