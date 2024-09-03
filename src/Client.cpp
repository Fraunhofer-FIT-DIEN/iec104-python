/**
 * Copyright 2020-2024 Fraunhofer Institute for Applied Information Technology
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

Client::Client(const std::uint_fast16_t tick_rate_ms,
               const std::uint_fast16_t timeout_ms,
               std::shared_ptr<Remote::TransportSecurity> transport_security)
    : tickRate_ms(tick_rate_ms), commandTimeout_ms(timeout_ms),
      security(std::move(transport_security)) {
  if (tickRate_ms < 50)
    throw std::range_error("tickRate_ms must be 50 or greater");
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

  schedulePeriodicTask([this]() { scheduleDataPointTimer(); }, tickRate_ms);

  {
    std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
    for (auto &c : connections) {
      c->connect();
    }
  }

  DEBUG_PRINT(Debug::Client, "start] Started");
}

void Client::stop() {
  Module::ScopedGilRelease const scoped("Client.stop");

  // stop active connection management first
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

  // stop all connections
  disconnectAll();

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

bool Client::hasOpenConnections() const {
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (auto &c : connections) {
    if (c->isOpen()) {
      return true;
    }
  }
  return false;
}

std::uint_fast8_t Client::getOpenConnectionCount() const {
  std::uint_fast8_t count = 0;
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (auto &c : connections) {
    if (c->isOpen()) {
      count++;
    }
  }
  return count;
}

bool Client::hasActiveConnections() const {
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (auto &c : connections) {
    if (ConnectionState::OPEN == c->getState()) {
      return true;
    }
  }
  return false;
}

std::uint_fast8_t Client::getActiveConnectionCount() const {
  std::uint_fast8_t count = 0;
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (auto &c : connections) {
    if (ConnectionState::OPEN == c->getState()) {
      count++;
    }
  }
  return count;
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
    std::cerr << "[c104.Client] add_connection] Connection already exists"
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
      station->addPoint(io_address, type, 0, std::nullopt);
    } catch (const std::exception &e) {
      DEBUG_PRINT(Debug::Client, "on_new_point] Failed to add point: " +
                                     std::string(e.what()));
    }
  }
}

std::uint_fast16_t Client::getTickRate_ms() const { return tickRate_ms; }

void Client::scheduleDataPointTimer() {
  uint16_t counter = 0;
  auto now = std::chrono::steady_clock::now();

  for (const auto &c : getConnections()) {
    if (c->isOpen() && !c->isMuted()) {
      for (const auto &station : c->getStations()) {
        for (const auto &point : station->getPoints()) {
          auto next = point->nextTimerAt();
          if (next.has_value() && next.value() < now) {
            scheduleTask([point]() { point->onTimer(); }, counter++);
          }
        }
      }
    }
  }
}

void Client::schedulePeriodicTask(const std::function<void()> &task,
                                  int interval) {
  {
    if (interval < 50) {
      throw std::out_of_range(
          "The interval for periodic tasks must be 1000ms at minimum.");
    }
    auto periodic = std::make_shared<std::function<void()>>([]() {});
    *periodic = [this, task, interval, periodic]() {
      // Schedule next execution
      scheduleTask(*periodic, interval);
      task();
    };
    // Schedule first execution
    scheduleTask(*periodic, interval);
  }

  runThread_wait.notify_one();
}

void Client::scheduleTask(const std::function<void()> &task, int delay) {
  {
    std::lock_guard<std::mutex> lock(runThread_mutex);
    if (delay < 0) {
      tasks.push({task, std::chrono::steady_clock::time_point::min()});
    } else {
      tasks.push({task, std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(delay)});
    }
  }

  runThread_wait.notify_one();
}

void Client::thread_run() {
  bool const debug = DEBUG_TEST(Debug::Client);
  running.store(true);
  while (enabled.load()) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(runThread_mutex);
      if (tasks.empty()) {
        runThread_wait.wait(lock);
        continue;
      }

      auto now = std::chrono::steady_clock::now();
      if (now >= tasks.top().schedule_time) {
        auto delay = now - tasks.top().schedule_time;
        if (delay > TASK_DELAY_THRESHOLD) {
          DEBUG_PRINT_CONDITION(
              debug, Debug::Client,
              "Warning: Task started delayed by " +
                  std::to_string(
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          delay)
                          .count()) +
                  " ms");
        }
        task = tasks.top().function;
        tasks.pop();
      } else {
        runThread_wait.wait_until(lock, tasks.top().schedule_time);
        continue;
      }
    }

    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "[c104.Client] loop] Task aborted: " << e.what()
                << std::endl;
    }
  }
  if (!tasks.empty()) {
    DEBUG_PRINT_CONDITION(debug, Debug::Client,
                          "loop] Tasks dropped due to stop: " +
                              std::to_string(tasks.size()));
    std::priority_queue<Task> empty;
    std::swap(tasks, empty);
  }
  running.store(false);
}
