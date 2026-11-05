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
 * @file Client.cpp
 * @brief operate a scada unit
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <algorithm>

#include "Client.h"

#include "module/ScopedGilAcquire.h"
#include "module/ScopedGilRelease.h"
#include "module/ThreadOwner.h"
#include "remote/Helper.h"

using namespace Remote;
using namespace std::chrono_literals;

Client::Client(const std::uint_fast16_t tick_rate_ms,
               const std::uint_fast16_t timeout_ms,
               std::shared_ptr<Remote::TransportSecurity> transport_security)
    : tickRate_ms(tick_rate_ms), commandTimeout_ms(timeout_ms),
      security(std::move(transport_security)) {
  if (tickRate_ms < 50)
    throw std::range_error("tickRate_ms must be 50 or greater");
  if (security) {
    // freeze the configuration now, as documented, instead of at the first
    // add_connection(); also rejects a half-loaded own certificate early
    security->get();
  }
  DEBUG_PRINT(Debug::Client, "Created");
}

Client::~Client() {
  // The last reference may have been dropped by a python callback, so this
  // can run on a connection's receive thread (on_new_station, on_new_point,
  // on_station_initialized) or on the run thread itself. Neither may be
  // joined from itself, see Module::ScopedThreadOwner.
  bool const onRunThread =
      runThread && runThread->get_id() == std::this_thread::get_id();

  enabled.store(false);
  if (onRunThread) {
    // thread_run() returns right after the current task, see ownerDestroyed()
    Module::ScopedThreadOwner::markDestroyed(this);
    runThread->detach();
    delete runThread;
    runThread = nullptr;
  } else {
    // the run thread may be waiting for the GIL inside a python callback, so
    // it must be joined without holding it (pybind11 destroys with the GIL)
    Module::ScopedGilRelease const scoped("Client.destroy");
    stopRunThread();
  }

  // close all connections; the one whose receive thread we are running on
  // cannot be closed here (CS104_Connection_close() would self-join), it is
  // closed on a neutral thread once the handler returned
  Remote::ConnectionVector deferred;
  {
    Module::ScopedGilRelease const scoped("Client.destroy");
    std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
    for (auto &c : connections) {
      if (Module::ScopedThreadOwner::isCurrentThreadOwnedBy(
              c->getCallbackKey())) {
        deferred.push_back(c);
      } else {
        c->disconnect();
      }
    }
    connections.clear();
  }
  if (!deferred.empty()) {
    std::thread([deferred = std::move(deferred)]() mutable {
      for (auto &c : deferred) {
        c->disconnect();
      }
      // may run ~Connection() here, on a thread lib60870 can be joined from
      deferred.clear();
    }).detach();
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

  std::weak_ptr<Client> weakSelf =
      shared_from_this(); // Weak reference to `this`
  schedulePeriodicTask(
      [weakSelf]() {
        auto self = weakSelf.lock();
        if (!self)
          return; // Prevent running if `this` was destroyed

        self->scheduleDataPointTimer();
      },
      tickRate_ms);

  {
    std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
    for (auto &c : connections) {
      c->connect();
    }
  }

  DEBUG_PRINT(Debug::Client, "start] Started");
}

void Client::stopRunThread() {
  // enabled is already false: pass through the mutex so that the run thread is
  // either before its predicate check or blocked in wait(), then wake it.
  // Notify unconditionally, running may not be set yet by a fresh thread.
  { std::lock_guard<std::mutex> const lock(runThread_mutex); }
  runThread_wait.notify_all();

  if (runThread) {
    runThread->join();
    delete runThread;
    runThread = nullptr;
  }
}

void Client::stop() {
  Module::ScopedGilRelease const scoped("Client.stop");

  // stop active connection management first
  bool expected = true;
  if (!enabled.compare_exchange_strong(expected, false)) {
    DEBUG_PRINT(Debug::Client, "stop] Already stopped");
    return;
  }

  stopRunThread();

  // stop all connections
  disconnectAll();

  DEBUG_PRINT(Debug::Client, "stop] Stopped");
}

bool Client::isRunning() { return enabled; }

void Client::setOriginatorAddress(uint_fast8_t address) {
  std::uint_fast8_t const prev = originatorAddress.exchange(address);
  if (prev != address) {
    // iterate a snapshot taken under connections_mutex: a concurrent
    // add_connection()/remove_connection() must not invalidate the loop, and
    // Connection::setOriginatorAddress() takes its own lock (no nesting)
    for (auto &c : getConnections()) {
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

bool Client::hasConnections() const {
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
  std::string const conStr = Remote::connectionStringFormatter(ip, port);

  // check and insert under the same lock, concurrent calls must not add the
  // same connection twice
  std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
  for (const auto &c : connections) {
    if (c->getConnectionString() == conStr) {
      std::cerr << "[c104.Client] add_connection] Connection already exists"
                << std::endl;
      return {nullptr};
    }
  }

  DEBUG_PRINT(Debug::Client,
              "add_connection] IP " + ip + " | PORT " + std::to_string(port));

  auto connection = Remote::Connection::create(
      shared_from_this(), ip, port, commandTimeout_ms, init, security,
      originatorAddress.load());
  connections.push_back(connection);

  return connection;
}

bool Client::removeConnection(const std::string &ip,
                              const std::uint_fast16_t port) {
  std::string const conStr = Remote::connectionStringFormatter(ip, port);

  std::shared_ptr<Remote::Connection> removed;
  {
    std::lock_guard<Module::GilAwareMutex> const lock(connections_mutex);
    const auto it = std::ranges::find_if(connections, [&conStr](const auto &c) {
      return c->getConnectionString() == conStr;
    });
    if (it == connections.end()) {
      return false;
    }
    removed = *it;
    connections.erase(it);
  }

  DEBUG_PRINT(Debug::Client, "remove_connection] IP " + ip + " | PORT " +
                                 std::to_string(port));

  // outside the lock: closing joins the receive thread, which may need it
  removed->detach();
  return true;
}

void Client::reconnectAll() const {
  Module::ScopedGilRelease const scoped("Client.reconnectAll");

  std::lock_guard<Module::GilAwareMutex> const con_lock(connections_mutex);
  for (auto &c : connections) {
    c->disconnect();
    c->connect();
  }
}

void Client::disconnectAll() const {
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

void Client::setOnEndOfInitializationCallback(py::object &callable) {
  py_onEndOfInitialization.reset(callable);
}

void Client::onEndOfInitialization(std::shared_ptr<Object::Station> station,
                                   CS101_CauseOfInitialization cause) {
  if (py_onEndOfInitialization.is_set()) {
    DEBUG_PRINT(Debug::Client, "CALLBACK on_station_initialized");
    Module::ScopedGilAcquire const scoped("Client.on_station_initialized");
    py_onEndOfInitialization.call(shared_from_this(), std::move(station),
                                  cause);
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

            std::weak_ptr<Object::DataPoint> weakPoint =
                point; // Weak reference to `point`
            scheduleTask(
                [weakPoint]() {
                  auto self = weakPoint.lock();
                  if (!self)
                    return; // Prevent running if `point` was destroyed

                  self->onTimer();
                },
                counter++);
          }
        }
      }
    }
  }
}

void Client::schedulePeriodicTask(const std::function<void()> &task,
                                  std::int_fast32_t interval) {
  if (interval < 50) {
    throw std::out_of_range(
        "The interval for periodic tasks must be 50ms at minimum.");
  }

  std::weak_ptr<Client> weakSelf =
      shared_from_this(); // Weak reference to `this`

  auto periodicCallback = [weakSelf, task, interval]() {
    auto self = weakSelf.lock();
    if (!self)
      return; // Prevent running if `this` was destroyed

    // Schedule next execution
    self->schedulePeriodicTask(task, interval); // Reschedule itself

    task();
  };
  // Schedule first execution
  scheduleTask(periodicCallback, interval);

  runThread_wait.notify_one();
}

std::size_t Client::getPendingTaskCount() const {
  std::lock_guard<std::mutex> const lock(runThread_mutex);
  return tasks.size();
}

std::size_t Client::runPendingTasksForTesting() {
  std::size_t executed = 0;
  while (true) {
    std::function<void()> task;
    {
      std::unique_lock<std::mutex> lock(runThread_mutex);
      if (tasks.empty() ||
          std::chrono::steady_clock::now() < tasks.top().schedule_time) {
        break; // nothing due, scheduled tasks stay queued
      }
      task = tasks.top().function;
      tasks.pop();
    }

    // same containment as thread_run(): a throwing task must not abort the
    // drain, and the queue entry is already gone
    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "[c104.Client] testing] Task aborted: " << e.what()
                << std::endl;
    }
    ++executed;
  }
  return executed;
}

std::size_t Client::discardPendingTasksForTesting() {
  std::lock_guard<std::mutex> const lock(runThread_mutex);
  std::size_t const discarded = tasks.size();
  if (discarded > 0) {
    // same discard as thread_run() on stop: the destroyed std::functions
    // release the notification tokens they captured
    std::priority_queue<Task> empty;
    std::swap(tasks, empty);
  }
  return discarded;
}

void Client::scheduleTask(const std::function<void()> &task,
                          std::int_fast32_t delay) {
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
  // lets ~Client() detect that it runs on this thread, see ownerDestroyed()
  Module::ScopedThreadOwner const owner(this);
  bool const debug = DEBUG_TEST(Debug::Client);
  running.store(true);
  while (enabled.load()) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(runThread_mutex);
      // predicates re-check the state under the lock: a notify_all() from
      // stopRunThread() that fires before wait() is entered is not lost
      if (tasks.empty()) {
        runThread_wait.wait(
            lock, [this] { return !enabled.load() || !tasks.empty(); });
        continue;
      }
      const auto top = tasks.top();

      auto now = std::chrono::steady_clock::now();
      if (now >= top.schedule_time) {
        auto delay = now - top.schedule_time;
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
        task = top.function;
        tasks.pop();
      } else {
        runThread_wait.wait_until(lock, top.schedule_time, [this, &top] {
          return !enabled.load() ||
                 tasks.top().schedule_time < top.schedule_time;
        });
        continue;
      }
    }

    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "[c104.Client] loop] Task aborted: " << e.what()
                << std::endl;
    }

    if (owner.ownerDestroyed()) {
      // the task dropped the last reference to this client: ~Client()
      // detached this thread, the object is gone, leave without touching it
      return;
    }
  }
  {
    std::unique_lock<std::mutex> lock(runThread_mutex);
    if (!tasks.empty()) {
      DEBUG_PRINT_CONDITION(debug, Debug::Client,
                            "loop] Tasks dropped due to stop: " +
                                std::to_string(tasks.size()));
      std::priority_queue<Task> empty;
      std::swap(tasks, empty);
    }
  }
  running.store(false);
}
