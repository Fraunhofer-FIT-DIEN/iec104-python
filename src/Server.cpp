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
 * @file Server.cpp
 * @brief operate a remote terminal unit
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Server.h"
#include "module/ScopedGilAcquire.h"
#include "module/ScopedGilRelease.h"
#include "module/ThreadOwner.h"
#include "remote/Helper.h"
#include "remote/TransportSecurity.h"
#include "remote/message/Batch.h"
#include "remote/message/PointCommand.h"
#include "remote/message/PointMessage.h"
#include <pybind11/chrono.h>
#include <type_traits>

using namespace Remote;
using namespace std::chrono_literals;

template <typename T>
bool areKeysSequential(const std::map<uint_fast16_t, T> &inputMap) {
  if (inputMap.empty()) {
    return true; // An empty map is trivially sequential
  }

  // Iterator-based traversal to test key sequences
  // std::map is always ordered, therefore sorting is not necessary
  auto it = inputMap.begin();
  uint_fast16_t previousKey = it->first;
  ++it;

  for (; it != inputMap.end(); ++it) {
    if (it->first != previousKey + 1) {
      return false; // Keys are not sequential
    }
    previousKey = it->first;
  }

  return true; // All keys are sequential
}

// Define static members
std::unordered_map<void *, std::weak_ptr<Server>> Server::instanceMap;
std::mutex Server::instanceMapMutex;

Server::Server(const std::string &bind_ip, const std::uint_fast16_t tcp_port,
               const std::uint_fast16_t tick_rate_ms,
               const std::uint_fast16_t select_timeout_ms,
               const std::uint_fast8_t max_open_connections,
               std::shared_ptr<Remote::TransportSecurity> transport_security)
    : ip(bind_ip), port(tcp_port), tickRate_ms(tick_rate_ms),
      selectTimeout_ms(select_timeout_ms),
      security(std::move(transport_security)),
      instanceKey(Module::nextCallbackKey()),
      maxOpenConnections(max_open_connections) {
  if (tickRate_ms < 50)
    throw std::range_error("tickRate_ms must be 50 or greater");
  Assert_IPv4(bind_ip);
  Assert_Port(tcp_port);

  // create a new slave/server instance with default connection parameters and
  // default message queue size
  if (security) {
    slave = CS104_Slave_createSecure(100, 100, security->get());
  } else {
    slave = CS104_Slave_create(100, 100);
  }

  // bind to ip addresses or all if bind_ip="0.0.0.0"
  CS104_Slave_setLocalAddress(slave, bind_ip.c_str());
  CS104_Slave_setLocalPort(slave, tcp_port);

  // Set mode to a multi redundancy group - NOTE: library has to be compiled
  // with CONFIG_CS104_SUPPORT_SERVER_MODE_SINGLE_REDUNDANCY_GROUP enabled (=1)
  // for single redundancy
  CS104_Slave_setServerMode(slave, CS104_MODE_CONNECTION_IS_REDUNDANCY_GROUP);

  CS104_Slave_setMaxOpenConnections(slave, max_open_connections);

  appLayerParameters = CS104_Slave_getAppLayerParameters(slave);

  // use lib60870-C defaults for connection timeouts
  // auto param = CS104_Slave_getConnectionParameters(slave);
  // param->t0 = 10; // socket connect timeout
  // param->t1 = 15; // acknowledgement timeout
  // param->t2 = 10; // acknowledgement interval

  // Function pointers for custom handler functions
  void *key = instanceKey;
  CS104_Slave_setConnectionRequestHandler(
      slave, &Server::connectionRequestHandler, key);
  CS104_Slave_setConnectionEventHandler(slave, &Server::connectionEventHandler,
                                        key);
  CS104_Slave_setRawMessageHandler(slave, &Server::rawMessageHandler, key);
  CS104_Slave_setInterrogationHandler(slave, &Server::interrogationHandler,
                                      key);
  CS104_Slave_setCounterInterrogationHandler(
      slave, &Server::counterInterrogationHandler, key);
  CS104_Slave_setReadHandler(slave, &Server::readHandler, key);
  CS104_Slave_setASDUHandler(slave, &Server::asduHandler, key);

  DEBUG_PRINT(Debug::Server, "Created");
}

Server::~Server() {
  // unregister first: lib60870 callbacks arriving from now on find no instance
  {
    std::lock_guard<std::mutex> lock(instanceMapMutex);
    instanceMap.erase(instanceKey);
  }

  // The last reference may have been dropped by a python callback, so this
  // can run on a lib60870 thread (listening or connection thread) or on the
  // run thread itself. Neither may be joined from itself: CS104_Slave_stop()
  // would self-join and then free the stack the current thread still executes
  // on, std::thread::join() would throw. Hand such work to a neutral thread.
  bool const onRunThread =
      runThread && runThread->get_id() == std::this_thread::get_id();
  bool const onCallbackThread =
      !onRunThread &&
      Module::ScopedThreadOwner::isCurrentThreadOwnedBy(instanceKey);

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
    Module::ScopedGilRelease const scoped("Server.destroyRunThread");
    stopRunThread();
  }

  {
    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    stations.clear();
  }

  {
    std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);
    connectionMap.clear();
  }
  clearSelections();

  if (onCallbackThread) {
    // stop joins the current thread as soon as the handler returned; the tls
    // configuration must outlive the slave, which holds a raw pointer to it
    std::thread([slave = this->slave, security = this->security]() {
      CS104_Slave_stop(slave);
      CS104_Slave_destroy(slave);
    }).detach();
  } else {
    // joins lib60870 threads, which may be waiting for the GIL in a callback
    Module::ScopedGilRelease const scoped("Server.destroy");
    CS104_Slave_stop(slave);
    CS104_Slave_destroy(slave);
  }
  slave = nullptr;

  DEBUG_PRINT(Debug::Server, "Removed");
}

std::shared_ptr<Server> Server::getInstance(void *key) {
  std::lock_guard<std::mutex> lock(instanceMapMutex);
  auto it = instanceMap.find(key);
  if (it != instanceMap.end()) {
    return it->second.lock();
  }
  return {nullptr};
}

std::string Server::getIP() const { return ip; }

std::uint_fast16_t Server::getPort() const { return port; }

void Server::setMaxOpenConnections(std::uint_fast8_t max_open_connections) {
  // lib60870 reads its limit unsynchronised from the server thread and must
  // only be configured before start(): just store the value here. start()
  // hands it to lib60870, connectionRequestHandler() enforces later changes.
  maxOpenConnections.store(max_open_connections);
}

std::uint_fast8_t Server::getMaxOpenConnections() const {
  return maxOpenConnections;
}

void Server::start() {
  bool expected = false;
  if (!enabled.compare_exchange_strong(expected, true)) {
    DEBUG_PRINT(Debug::Server, "start] Already running");
    return;
  }

  Module::ScopedGilRelease const scoped("Server.start");

  CS104_Slave_setMaxOpenConnections(slave, maxOpenConnections.load());
  CS104_Slave_start(slave);

  if (!CS104_Slave_isRunning(slave)) {
    enabled.store(false);
    throw std::runtime_error("Can't start server: Port in use?");
  }

  DEBUG_PRINT(Debug::Server, "start] Started");

  if (!runThread) {
    runThread = new std::thread(&Server::thread_run, this);
  }

  // Schedule periodics based on tickRate
  std::weak_ptr<Server> weakSelf =
      shared_from_this(); // Weak reference to `this`
  schedulePeriodicTask(
      [weakSelf]() {
        auto self = weakSelf.lock();
        if (!self)
          return; // Prevent running if `this` was destroyed

        self->sendPeriodicInventory();
      },
      tickRate_ms);

  schedulePeriodicTask(
      [weakSelf]() {
        auto self = weakSelf.lock();
        if (!self)
          return; // Prevent running if `this` was destroyed

        self->cleanupSelections();
        self->scheduleDataPointTimer();
      },
      tickRate_ms);
}

void Server::scheduleDataPointTimer() {
  if (!hasActiveConnections())
    return;

  uint16_t counter = 0;
  auto now = std::chrono::steady_clock::now();
  for (const auto &station : getStations()) {
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

void Server::schedulePeriodicTask(const std::function<void()> &task,
                                  std::int_fast32_t interval) {
  if (interval < 50) {
    throw std::out_of_range(
        "The interval for periodic tasks must be 50ms at minimum.");
  }

  std::weak_ptr<Server> weakSelf =
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

void Server::scheduleTask(const std::function<void()> &task,
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

void Server::thread_run() {
  // lets ~Server() detect that it runs on this thread, see ownerDestroyed()
  Module::ScopedThreadOwner const owner(this);
  bool const debug = DEBUG_TEST(Debug::Server);
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
              debug, Debug::Server,
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
      std::cerr << "[c104.Server] loop] Task aborted: " << e.what()
                << std::endl;
    }

    if (owner.ownerDestroyed()) {
      // the task dropped the last reference to this server: ~Server()
      // detached this thread, the object is gone, leave without touching it
      return;
    }
  }
  {
    std::unique_lock<std::mutex> lock(runThread_mutex);
    if (!tasks.empty()) {
      DEBUG_PRINT_CONDITION(debug, Debug::Server,
                            "loop] Tasks dropped due to stop: " +
                                std::to_string(tasks.size()));
      std::priority_queue<Task> empty;
      std::swap(tasks, empty);
    }
  }
  running.store(false);
}

void Server::stopRunThread() {
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

void Server::stop() {
  Module::ScopedGilRelease const scoped("Server.stop");
  bool expected = true;
  if (!enabled.compare_exchange_strong(expected, false)) {
    DEBUG_PRINT(Debug::Server, "stop] Already stopped");
    return;
  }

  stopRunThread();

  CS104_Slave_stop(slave);

  {
    std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);
    connectionMap.clear();
  }
  clearSelections();
  activeConnections.store(0);
  openConnections.store(0);

  DEBUG_PRINT(Debug::Server, "stop] Stopped");
}

bool Server::isRunning() const {
  return enabled && CS104_Slave_isRunning(slave);
}

bool Server::hasStations() const {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  return !stations.empty();
}

bool Server::isExistingConnection(IMasterConnection connection) {
  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);

  auto it = connectionMap.find(connection);
  return it != connectionMap.end();
}

std::optional<std::uint64_t>
Server::getConnectionId(IMasterConnection connection) const {
  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);

  auto it = connectionMap.find(connection);
  if (it == connectionMap.end()) {
    return std::nullopt;
  }
  return it->second.id;
}

bool Server::hasOpenConnections() const {
  return CS104_Slave_getOpenConnections(slave) > 0;
}

std::uint_fast8_t Server::getOpenConnectionCount() const {
  return CS104_Slave_getOpenConnections(slave);
}

bool Server::hasActiveConnections() const {
  return activeConnections.load() > 0;
}

std::uint_fast8_t Server::getActiveConnectionCount() const {
  return activeConnections.load();
}

Object::StationVector Server::getStations() const {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  return stations;
}

std::shared_ptr<Object::Station>
Server::getStation(const std::uint_fast16_t commonAddress) const {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  for (auto &s : stations) {
    if (s->getCommonAddress() == commonAddress) {
      return s;
    }
  }

  return {nullptr};
}

bool Server::hasStation(const std::uint_fast16_t commonAddress) const {

  if (IEC60870_GLOBAL_COMMON_ADDRESS == commonAddress) {
    return true;
  }

  return nullptr != getStation(commonAddress).get();
}

std::shared_ptr<Object::Station>
Server::addStation(std::uint_fast16_t commonAddress) {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  for (auto &s : stations) {
    if (s->getCommonAddress() == commonAddress) {
      return {nullptr};
    }
  }

  auto station = Object::Station::create(commonAddress, shared_from_this());
  DEBUG_PRINT(Debug::Server,
              "add_station] CA " + std::to_string(commonAddress));
  stations.push_back(station);
  return station;
}

bool Server::removeStation(std::uint_fast16_t commonAddress) {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  DEBUG_PRINT(Debug::Server,
              "remove_station] CA " + std::to_string(commonAddress));

  size_t originalSize = stations.size();

  std::erase_if(stations, [commonAddress](
                              const std::shared_ptr<Object::Station> &station) {
    // Check if the current DataPoint matches the provided address
    if (station->getCommonAddress() == commonAddress) {
      station->detach();
      return true;
    }
    return false;
  });

  return (stations.size() < originalSize); // Success if the size decreased
}

void Server::cleanupSelections() {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  std::erase_if(selectionVector, [this, now](const Selection &s) {
    if ((now - s.created) < selectTimeout_ms) {
      return false;
    } else {
      unselect(s);
      return true;
    }
  });
}

void Server::dropConnectionSelections(const std::uint64_t connectionId) {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  std::erase_if(selectionVector, [connectionId](const Selection &s) {
    if (s.connectionId != connectionId) {
      return false;
    }
    // no unselect: the connection is gone,
    // nobody is left to receive the
    // activation termination
    CS101_ASDU_destroy(s.asdu);
    return true;
  });
}

void Server::clearSelections() {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  for (const auto &s : selectionVector) {
    CS101_ASDU_destroy(s.asdu);
  }
  selectionVector.clear();
}

void Server::cleanupSelection(uint16_t ca, uint32_t ioa) {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  std::erase_if(selectionVector, [this, ca, ioa](const Selection &s) {
    if (s.ca == ca && s.ioa == ioa) {
      unselect(s);
      return true;
    } else {
      return false;
    }
  });
}

std::optional<uint8_t> Server::getSelector(const uint16_t ca,
                                           const uint32_t ioa) {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it =
      std::ranges::find_if(selectionVector, [ca, ioa](const Selection &s) {
        return s.ca == ca && s.ioa == ioa;
      });

  // selection NOT found
  if (it != selectionVector.end() && (now - it->created) < selectTimeout_ms) {
    return it->oa;
  }
  return std::nullopt;
}

bool Server::select(
    IMasterConnection connection,
    const std::shared_ptr<Remote::Message::IncomingMessage> &message) {
  const auto type = message->getType();
  if (type < C_SC_NA_1 || C_BO_NA_1 == type || C_SE_TC_1 < type) {
    throw std::invalid_argument(
        "Only control points, except for binary commands can be selected");
  }

  const uint8_t oa = message->getOriginatorAddress();
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();

  // resolve the stable id before locking selection_mutex (lock order:
  // connection_mutex is never taken while selection_mutex is held)
  const auto connectionId = getConnectionId(connection);
  if (!connectionId.has_value()) {
    DEBUG_PRINT(Debug::Server, "select] Unknown connection, rejected");
    return false;
  }
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it =
      std::ranges::find_if(selectionVector, [ca, ioa](const Selection &s) {
        return s.ca == ca && s.ioa == ioa;
      });

  // selection NOT found
  if (it == selectionVector.end()) {
    CS101_ASDU cp = Remote::cloneAsdu(message->getAsdu(), appLayerParameters);
    selectionVector.emplace_back<Selection>(
        {cp, oa, ca, ioa, connection, connectionId.value(), now});
    return true;
  }

  // selection found
  const bool owner = (it->connectionId == connectionId.value());
  const bool expired = (now - it->created) >= selectTimeout_ms;
  if (!owner && !expired) {
    return false;
  }

  if (owner) {
    // the same master re-selects: supersede silently
    CS101_ASDU_destroy(it->asdu);
  } else {
    // expired selection of another master: terminate it (as the periodic
    // cleanup would have done) and transfer the ownership completely
    unselect(*it);
  }
  it->asdu = Remote::cloneAsdu(message->getAsdu(), appLayerParameters);
  it->oa = oa;
  it->connection = connection;
  it->connectionId = connectionId.value();
  it->created = now;

  return true;
}

void Server::unselect(const Selection &selection) {
  std::weak_ptr<Server> weakSelf =
      shared_from_this(); // Weak reference to `this`

  // the task owns the cloned asdu, so it is freed even if the task is dropped
  // because the server stopped before it ran
  std::shared_ptr<std::remove_pointer_t<CS101_ASDU>> const asdu(
      selection.asdu, [](CS101_ASDU a) { CS101_ASDU_destroy(a); });
  IMasterConnection const connection = selection.connection;
  std::uint64_t const connectionId = selection.connectionId;

  scheduleTask([weakSelf, asdu, connection, connectionId]() {
    auto self = weakSelf.lock();
    if (!self)
      return; // Prevent running if `this` was destroyed

    // only the peer that selected may receive the termination, not a new
    // peer that lib60870 handed the same connection object to
    self->sendActivationTermination(connection, asdu.get(), connectionId);
  });
}

bool Server::cancelSelection(
    IMasterConnection connection,
    const std::shared_ptr<Remote::Message::IncomingMessage> &message) {
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();

  // see select(): resolve the id before locking selection_mutex (lock order:
  // connection_mutex is never taken while selection_mutex is held)
  const auto connectionId = getConnectionId(connection);
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it =
      std::ranges::find_if(selectionVector, [ca, ioa](const Selection &s) {
        return s.ca == ca && s.ioa == ioa;
      });

  // only the requester's own, still valid selection may be cancelled; a
  // missing, foreign, unknown or expired selection is left untouched
  if (it == selectionVector.end() || !connectionId.has_value() ||
      it->connectionId != connectionId.value() ||
      (now - it->created) >= selectTimeout_ms) {
    return false;
  }

  // the cloned asdu belongs to the selection and is freed with it; no
  // unselect(): a termination would announce an execution that never happens,
  // the caller's DEACT_CON already reports the cancellation
  CS101_ASDU_destroy(it->asdu);
  selectionVector.erase(it);
  return true;
}

CommandResponseState
Server::execute(IMasterConnection connection,
                std::shared_ptr<Remote::Message::IncomingMessage> message,
                std::shared_ptr<Object::DataPoint> point) {
  bool selected = false;
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();

  if (SELECT_AND_EXECUTE_COMMAND == point->getCommandMode()) {
    // see select(): resolve the id before locking selection_mutex
    const auto connectionId = getConnectionId(connection);
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
    auto it =
        std::ranges::find_if(selectionVector, [ca, ioa](const Selection &s) {
          return s.ca == ca && s.ioa == ioa;
        });

    // selection must exist, be owned by this very connection and be valid
    selected = (it != selectionVector.end() && connectionId.has_value() &&
                (it->connectionId == connectionId.value()) &&
                (now - it->created) < selectTimeout_ms);
    if (!selected) {
      std::cerr << "[c104.Server] Cannot execute command on point in "
                   "SELECT_AND_EXECUTE "
                   "command mode without selection"
                << std::endl;
      return RESPONSE_STATE_FAILURE;
    }
  }

  auto res = point->onReceive(std::move(message));

  if (selected) {
    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    scheduleTask(
        [weakSelf, ca, ioa]() {
          auto self = weakSelf.lock();
          if (!self)
            return; // Prevent running if `this` was destroyed

          self->cleanupSelection(ca, ioa);
        },
        1);
  }

  return res;
}

std::shared_ptr<ProtocolParameters> Server::getParameters() const {
  // the view holds a strong reference, so capturing `this` is safe
  return std::make_shared<ProtocolParameters>(
      CS104_Slave_getConnectionParameters(slave), shared_from_this(),
      [this]() { return isRunning(); },
      "while the server is running, call Server.stop() first");
}

void Server::setOnReceiveRawCallback(py::object &callable) {
  py_onReceiveRaw.reset(callable);
}

void Server::onReceiveRaw(unsigned char *msg, int msgSize) {
  if (msgSize <= 0 || !msg) {
    return;
  }
  if (py_onReceiveRaw.is_set()) {
    auto token = acquireNotificationToken("on_receive_raw");
    if (!token) {
      return; // dropped, the callback cannot keep up with the frame rate
    }
    // create a copy
    const auto size = static_cast<std::size_t>(msgSize);
    auto cp = std::shared_ptr<char[]>(new char[size]);
    memcpy(cp.get(), msg, size);

    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    scheduleTask([weakSelf, cp, size, token]() {
      auto self = weakSelf.lock();
      if (!self)
        return; // Prevent running if `this` was destroyed

      DEBUG_PRINT(Debug::Server, "CALLBACK on_receive_raw");
      Module::ScopedGilAcquire const scoped("Server.on_receive_raw");
      self->py_onReceiveRaw.call(self, py::bytes(cp.get(), size));
    });
  }
}

void Server::setOnSendRawCallback(py::object &callable) {
  py_onSendRaw.reset(callable);
}

void Server::onSendRaw(unsigned char *msg, int msgSize) {
  if (msgSize <= 0 || !msg) {
    return;
  }
  if (py_onSendRaw.is_set()) {
    auto token = acquireNotificationToken("on_send_raw");
    if (!token) {
      return; // dropped, the callback cannot keep up with the frame rate
    }
    // create a copy
    const auto size = static_cast<std::size_t>(msgSize);
    auto cp = std::shared_ptr<char[]>(new char[size]);
    memcpy(cp.get(), msg, size);

    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    scheduleTask([weakSelf, cp, size, token]() {
      auto self = weakSelf.lock();
      if (!self)
        return; // Prevent running if `this` was destroyed

      DEBUG_PRINT(Debug::Server, "CALLBACK on_send_raw");
      Module::ScopedGilAcquire const scoped("Server.on_send_raw");
      self->py_onSendRaw.call(self, py::bytes(cp.get(), size));
    });
  }
}

void Server::setOnClockSyncCallback(py::object &callable) {
  py_onClockSync.reset(callable);
}

CommandResponseState
Server::onClockSync(const std::string _ip,
                    const std::chrono::system_clock::time_point time) {
  if (py_onClockSync.is_set()) {
    DEBUG_PRINT(Debug::Server, "CALLBACK on_clock_sync");
    Module::ScopedGilAcquire const scoped("Server.on_clock_sync");
    PyDateTime_IMPORT;

    // pybind11/chrono.h caster code copy
    if (!PyDateTimeAPI) {
      PyDateTime_IMPORT;
    }

    using us_t = std::chrono::duration<int, std::micro>;
    auto us = std::chrono::duration_cast<us_t>(time.time_since_epoch() %
                                               std::chrono::seconds(1));
    if (us.count() < 0) {
      us += std::chrono::seconds(1);
    }

    std::time_t tt = std::chrono::system_clock::to_time_t(
        std::chrono::time_point_cast<std::chrono::system_clock::duration>(time -
                                                                          us));

    // Use the thread-safe localtime
    // todo use UTC in future?
    std::tm localtime{};
    if (!localtime_safe(&tt, &localtime)) {
      return RESPONSE_STATE_FAILURE;
    }

    // take ownership of the new reference, otherwise each clock sync leaks it
    auto pydate = py::reinterpret_steal<py::object>(PyDateTime_FromDateAndTime(
        localtime.tm_year + 1900, localtime.tm_mon + 1, localtime.tm_mday,
        localtime.tm_hour, localtime.tm_min, localtime.tm_sec, us.count()));
    if (!pydate) {
      PyErr_Clear();
      return RESPONSE_STATE_FAILURE;
    }

    // fail closed: a raising handler or an invalid return value rejects the
    // clock sync, the handler itself stays registered
    return py_onClockSync.call(shared_from_this(), _ip, pydate)
        .value_or(RESPONSE_STATE_FAILURE);
  }

  return RESPONSE_STATE_SUCCESS;
}

void Server::setOnUnexpectedMessageCallback(py::object &callable) {
  py_onUnexpectedMessage.reset(callable);
}

void Server::onUnexpectedMessage(
    IMasterConnection connection,
    std::shared_ptr<Remote::Message::IncomingMessage> message,
    UnexpectedMessageCause cause) {
  CS101_ASDU asdu = message->getAsdu();

  // manipulate and send copy instead of original ASDU
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setNegative(cp, true);
  switch (cause) {
  case INVALID_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Invalid type id");
    CS101_ASDU_setCOT(cp, CS101_COT_UNKNOWN_TYPE_ID);
    IMasterConnection_sendASDU(connection, cp);
    break;
  case MISMATCHED_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Mismatching type id");
    CS101_ASDU_setCOT(cp, CS101_COT_UNKNOWN_TYPE_ID);
    IMasterConnection_sendASDU(connection, cp);
    break;
  case UNKNOWN_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unknown type id");
    CS101_ASDU_setCOT(cp, CS101_COT_UNKNOWN_TYPE_ID);
    IMasterConnection_sendASDU(connection, cp);
    break;
  case INVALID_COT:
  case UNKNOWN_COT:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Invalid COT");
    CS101_ASDU_setCOT(cp, CS101_COT_UNKNOWN_COT);
    IMasterConnection_sendASDU(connection, cp);
    break;
  case UNKNOWN_CA:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unknown CA");
    CS101_ASDU_setCOT(cp, CS101_COT_UNKNOWN_CA);
    IMasterConnection_sendASDU(connection, cp);
    break;
  case UNKNOWN_IOA:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unknown IOA");
    CS101_ASDU_setCOT(cp, CS101_COT_UNKNOWN_IOA);
    IMasterConnection_sendASDU(connection, cp);
    break;
  case UNIMPLEMENTED_GROUP:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unimplemented group");
    break;
  default: {
  }
  }
  CS101_ASDU_destroy(cp);

  if (py_onUnexpectedMessage.is_set()) {
    auto token = acquireNotificationToken("on_unexpected_message");
    if (!token) {
      return; // dropped, the callback cannot keep up with the frame rate
    }
    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    // snapshot: the task runs on another thread than the message handler
    scheduleTask([weakSelf, message = message->snapshot(), cause, token]() {
      auto self = weakSelf.lock();
      if (!self)
        return; // Prevent running if `this` was destroyed

      DEBUG_PRINT(Debug::Server, "CALLBACK on_unexpected_message");
      Module::ScopedGilAcquire const scoped("Server.on_unexpected_message");
      self->py_onUnexpectedMessage.call(self, message, cause);
    });
  }
}

void Server::setOnConnectCallback(py::object &callable) {
  py_onConnect.reset(callable);
}

std::uint_fast16_t Server::getTickRate_ms() const { return tickRate_ms; }

bool Server::connectionRequestHandler(void *parameter, const char *ipAddress) {
  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Reject connection request in shutdown");
    return false;
  }

  // max_connections may have been lowered while running, see
  // setMaxOpenConnections()
  const auto maxOpen = instance->maxOpenConnections.load();
  if (maxOpen > 0 &&
      CS104_Slave_getOpenConnections(instance->slave) >= maxOpen) {
    DEBUG_PRINT(Debug::Server, "Reject connection request: max_connections");
    return false;
  }

  if (instance->py_onConnect.is_set()) {
    DEBUG_PRINT(Debug::Server, "CALLBACK on_connect");
    Module::ScopedGilAcquire const scoped("Server.on_connect");
    std::string const ip(ipAddress);
    // fail closed: a raising handler or an invalid return value rejects the
    // connection, the handler itself stays registered
    return instance->py_onConnect.call(instance, ip).value_or(false);
  }

  return true;
}

void Server::connectionEventHandler(void *parameter,
                                    IMasterConnection connection,
                                    CS104_PeerConnectionEvent event) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore connection event " +
                                   PeerConnectionEvent_toString(event) +
                                   " in shutdown");
    return;
  }

  char ipAddrStr[60];
  IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

  // id of a connection that ended with this event, its selections are
  // dropped below, outside of connection_mutex (lock order, see select())
  std::optional<std::uint64_t> closedId;
  {
    std::lock_guard<Module::GilAwareMutex> const lock(
        instance->connection_mutex);

    if (event == CS104_CON_EVENT_CONNECTION_OPENED) {
      // set as invalid receiver
      auto it = instance->connectionMap.find(connection);
      if (it == instance->connectionMap.end()) {
        instance->connectionMap.emplace(
            connection, PeerConnectionState{.id = ++instance->lastConnectionId,
                                            .active = false});
      } else {
        // lib60870 reused the connection object for a new peer without a
        // CLOSED event for the old one: new identity, old selections end
        if (it->second.active) {
          --instance->activeConnections;
        }
        closedId = it->second.id;
        it->second = PeerConnectionState{.id = ++instance->lastConnectionId,
                                         .active = false};
      }

    } else if (event == CS104_CON_EVENT_CONNECTION_CLOSED) {
      // set as invalid receiver
      auto it = instance->connectionMap.find(connection);
      if (it != instance->connectionMap.end()) {
        if (it->second.active) {
          --instance->activeConnections;
        }
        closedId = it->second.id;
        instance->connectionMap.erase(it);
      }

    } else if (event == CS104_CON_EVENT_ACTIVATED) {
      // set as valid receiver
      if (auto it = instance->connectionMap.find(connection);
          it == instance->connectionMap.end()) {
        instance->connectionMap.emplace(
            connection, PeerConnectionState{.id = ++instance->lastConnectionId,
                                            .active = true});
        ++instance->activeConnections;
      } else {
        if (!it->second.active) {
          ++instance->activeConnections;
        }
        it->second.active = true;
      }

    } else if (event == CS104_CON_EVENT_DEACTIVATED) {
      // set as invalid receiver
      auto it = instance->connectionMap.find(connection);
      if (it == instance->connectionMap.end()) {
        instance->connectionMap.emplace(
            connection, PeerConnectionState{.id = ++instance->lastConnectionId,
                                            .active = false});
      } else if (it->second.active) {
        --instance->activeConnections;
        it->second.active = false;
      }
    }
  }

  if (closedId.has_value()) {
    // synchronously: a new peer may receive the same connection object right
    // after this event and must not inherit a selection
    instance->dropConnectionSelections(closedId.value());
  }

  if (debug) {
    const std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "connection_event_handler] Connection " +
                              PeerConnectionEvent_toString(event) + " by " +
                              std::string(ipAddrStr) + " | TOTAL " +
                              TICTOC(begin, end));
  }
}

bool Server::transmit(std::shared_ptr<Object::DataPoint> point,
                      const CS101_CauseOfTransmission cause,
                      const uint_fast8_t originator,
                      IMasterConnection connection,
                      const std::optional<std::uint64_t> connectionId) {
  auto type = point->getType();

  // report monitoring point
  if (type < S_IT_TC_1) {
    auto message = Message::PointMessage::create(std::move(point));
    message->setCauseOfTransmission(cause);
    message->setOriginatorAddress(originator);
    return send(std::move(message), connection, connectionId);
  }

  // lazy confirm previous control command
  if (type > S_IT_TC_1 && type < M_EI_NA_1) {
    auto message = Message::PointCommand::create(std::move(point));
    message->setCauseOfTransmission(cause);
    message->setOriginatorAddress(originator);
    return send(std::move(message), connection, connectionId);
  }

  throw std::invalid_argument("Invalid point type");
}

bool Server::send(std::shared_ptr<Remote::Message::OutgoingMessage> message,
                  IMasterConnection connection,
                  const std::optional<std::uint64_t> connectionId) {
  if (!enabled.load() || !hasActiveConnections())
    return false;

  Module::ScopedGilRelease const scoped("Server.send");

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  CS101_ASDU asdu = CS101_ASDU_create(
      appLayerParameters, message->isSequence(),
      message->getCauseOfTransmission(), message->getOriginatorAddress(),
      message->getCommonAddress(), message->isTest(), message->isNegative());

  // @todo add support for packed messages / multiple IOs in outgoing message
  CS101_ASDU_addInformationObject(asdu, message->getInformationObject());

  // @todo high vs low priority messages
  bool sent = true;
  if (!connection || CS101_COT_PERIODIC == message->getCauseOfTransmission() ||
      CS101_COT_SPONTANEOUS == message->getCauseOfTransmission())
    // low priority
    CS104_Slave_enqueueASDU(slave, asdu);
  else {
    // high priority, only to the given connection: verified under
    // connection_mutex, so a closed or recycled connection is skipped
    sent = sendToConnection(connection, connectionId, asdu);
  }

  CS101_ASDU_destroy(asdu);
  if (!sent) {
    return false;
  }

  if (debug) {
    const std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "send] Send " +
                              std::string(TypeID_toString(message->getType())) +
                              " | COT: " +
                              CS101_CauseOfTransmission_toString(
                                  message->getCauseOfTransmission()) +
                              " | TOTAL " + TICTOC(begin, end));
  }

  return true;
}

bool Server::sendBatch(std::shared_ptr<Remote::Message::Batch> batch,
                       IMasterConnection connection) const {
  if (!enabled.load() || !hasActiveConnections())
    return false;

  Module::ScopedGilRelease const scoped("Server.sendBatch");

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  if (!batch->hasPoints()) {
    DEBUG_PRINT(Debug::Server, "Empty batch");
    return false;
  }

  CS101_ASDU asdu = CS101_ASDU_create(
      appLayerParameters, batch->isSequence(), batch->getCauseOfTransmission(),
      batch->getOriginatorAddress(), batch->getCommonAddress(), batch->isTest(),
      batch->isNegative());

  for (auto &point : batch->getPoints()) {

    // Update all (!) data points before transmitting them
    if (CS101_COT_PERIODIC == batch->getCauseOfTransmission()) {
      point->onBeforeAutoTransmit();
    } else {
      point->onBeforeRead();
    }

    try {
      // catch exception in create, if point was detached in meanwhile
      auto message = Remote::Message::PointMessage::create(point);

      // not added
      if (!CS101_ASDU_addInformationObject(asdu,
                                           message->getInformationObject())) {
        // ASDU packet size exceeded => send ASDU and create a new one
        // @todo high vs low priority messages
        if (!connection ||
            CS101_COT_PERIODIC == batch->getCauseOfTransmission() ||
            CS101_COT_SPONTANEOUS == batch->getCauseOfTransmission())
          // low priority
          CS104_Slave_enqueueASDU(slave, asdu);
        else {
          // high priority
          IMasterConnection_sendASDU(connection, asdu);
        }

        // recreate new asdu
        CS101_ASDU_destroy(asdu);
        asdu = CS101_ASDU_create(
            appLayerParameters, batch->isSequence(),
            batch->getCauseOfTransmission(), batch->getOriginatorAddress(),
            batch->getCommonAddress(), batch->isTest(), batch->isNegative());

        // add message to new asdu
        if (!CS101_ASDU_addInformationObject(asdu,
                                             message->getInformationObject())) {
          DEBUG_PRINT(Debug::Server, "Dropped message for inventory, "
                                     "cannot be added to new ASDU: " +
                                         std::to_string(message->getIOA()));
        }
      }

    } catch (const std::exception &e) {
      DEBUG_PRINT(Debug::Server,
                  "Skip invalid point (ioa: " +
                      std::to_string(point->getInformationObjectAddress()) +
                      ") in Batch: " + std::string(e.what()));
    }
  }

  // if ASDU is not empty, send ASDU
  if (CS101_ASDU_getNumberOfElements(asdu) > 0) {
    // @todo high vs low priority messages
    if (!connection || CS101_COT_PERIODIC == batch->getCauseOfTransmission() ||
        CS101_COT_SPONTANEOUS == batch->getCauseOfTransmission())
      // low priority
      CS104_Slave_enqueueASDU(slave, asdu);
    else {
      // high priority
      IMasterConnection_sendASDU(connection, asdu);
    }
  }

  // free ASDU memory
  CS101_ASDU_destroy(asdu);

  if (debug) {
    const std::chrono::steady_clock::time_point end =
        std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "send] Send Batch " +
                              std::string(TypeID_toString(batch->getType())) +
                              " | COT: " +
                              CS101_CauseOfTransmission_toString(
                                  batch->getCauseOfTransmission()) +
                              " | TOTAL " + TICTOC(begin, end));
  }

  return true;
}

void Server::sendActivationConfirmation(IMasterConnection connection,
                                        CS101_ASDU asdu, bool negative) {
  // manipulate copy instead of original asdu
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setCOT(cp, CS101_COT_ACTIVATION_CON);
  CS101_ASDU_setNegative(cp, negative);

  if (!sendToConnection(connection, std::nullopt, cp)) {
    DEBUG_PRINT(Debug::Server,
                "send_activation_confirmation] Connection not found, skip");
  }
  CS101_ASDU_destroy(cp);
}

void Server::sendDeactivationConfirmation(IMasterConnection connection,
                                          CS101_ASDU asdu, bool negative) {
  // manipulate copy instead of original asdu
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setCOT(cp, CS101_COT_DEACTIVATION_CON);
  CS101_ASDU_setNegative(cp, negative);

  if (!sendToConnection(connection, std::nullopt, cp)) {
    DEBUG_PRINT(Debug::Server,
                "send_deactivation_confirmation] Connection not found, skip");
  }
  CS101_ASDU_destroy(cp);
}

void Server::sendActivationTermination(
    IMasterConnection connection, CS101_ASDU asdu,
    const std::optional<std::uint64_t> connectionId) {
  // manipulate copy instead of original asdu
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setCOT(cp, CS101_COT_ACTIVATION_TERMINATION);

  if (!sendToConnection(connection, connectionId, cp)) {
    DEBUG_PRINT(Debug::Server,
                "send_activation_termination] Connection not found, skip");
  }
  CS101_ASDU_destroy(cp);
}

bool Server::sendToConnection(IMasterConnection connection,
                              const std::optional<std::uint64_t> connectionId,
                              CS101_ASDU asdu) {
  // collect the target common addresses first: station_mutex must not be
  // taken while connection_mutex is held
  const bool global = isGlobalCommonAddress(CS101_ASDU_getCA(asdu));
  std::vector<std::uint16_t> commonAddresses;
  if (global) {
    for (const auto &s : getStations()) {
      commonAddresses.push_back(s->getCommonAddress());
    }
  }

  // verify and send under the lock, so that the connection cannot be closed
  // and handed to a new peer in between (IMasterConnection_sendASDU only
  // enqueues and does not block)
  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);
  auto it = connectionMap.find(connection);
  if (it == connectionMap.end()) {
    return false;
  }
  if (connectionId.has_value() && it->second.id != connectionId.value()) {
    DEBUG_PRINT(Debug::Server, "send_to_connection] Connection object was "
                               "reused by another peer, skip");
    return false;
  }

  if (global) {
    DEBUG_PRINT(Debug::Server, "send_to_connection] to all MTUs (global CA)");
    for (const auto ca : commonAddresses) {
      CS101_ASDU_setCA(asdu, ca);
      IMasterConnection_sendASDU(connection, asdu);
    }
  } else {
    DEBUG_PRINT(Debug::Server, "send_to_connection] to requesting MTU CA=" +
                                   std::to_string(CS101_ASDU_getCA(asdu)));
    IMasterConnection_sendASDU(connection, asdu);
  }
  return true;
}

void Server::sendEndOfInitialization(
    const std::uint_fast16_t commonAddress,
    const CS101_CauseOfInitialization cause) const {

  Module::ScopedGilRelease const scoped("Server.send");

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  auto io = EndOfInitialization_create(nullptr, static_cast<uint8_t>(cause));

  CS101_ASDU asdu =
      CS101_ASDU_create(appLayerParameters, false, CS101_COT_INITIALIZED, 0,
                        commonAddress, false, false);
  CS101_ASDU_addInformationObject(asdu,
                                  reinterpret_cast<InformationObject>(io));
  CS104_Slave_enqueueASDU(slave, asdu);
  CS101_ASDU_destroy(asdu);
  EndOfInitialization_destroy(io);

  if (debug) {
    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "send] Send M_EI_NA_1 | COI: " + CauseOfInitialization_toString(cause) +
            " | TOTAL " + TICTOCNOW(begin));
  }
}

void Server::sendPeriodicInventory() {
  if (!enabled.load() || !hasActiveConnections())
    return;

  bool const debug = DEBUG_TEST(Debug::Server);

  // always initialize begin, as it is used for cyclic report interval
  const std::chrono::steady_clock::time_point begin =
      std::chrono::steady_clock::now();

  bool empty = true;
  IEC60870_5_TypeID type = C_TS_TA_1;

  // batch messages per station by type
  std::map<IEC60870_5_TypeID, std::shared_ptr<Remote::Message::Batch>> batchMap;

  for (const auto &station : getStations()) {
    for (const auto &point : station->getPoints()) {
      type = point->getType();

      // only monitoring points SP,DP,ST,ME,BO + IT
      if (type > M_IT_TB_1 || (type > M_IT_NA_1 && type < M_SP_TB_1))
        continue;

      // enabled cyclic report and ready for cyclic transmission ?
      const auto next = point->nextReportAt();
      if (!next.has_value() || begin < next.value())
        continue;

      try {
        // add message to group
        auto g = batchMap.find(type);
        if (g == batchMap.end()) {
          auto b = Remote::Message::Batch::create(CS101_COT_PERIODIC);
          b->addPoint(point);
          batchMap[type] = std::move(b);
          empty = false;
        } else {
          g->second->addPoint(point);
        }
      } catch (const std::exception &e) {
        DEBUG_PRINT(Debug::Server, "Invalid point message for inventory: " +
                                       std::string(e.what()));
      }
    }

    // send batched messages of current station
    for (auto &batch : batchMap) {
      sendBatch(std::move(batch.second));
    }
    batchMap.clear();
  }

  if (!empty && debug) {
    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "auto_transmit] TOTAL " + TICTOCNOW(begin));
  }
}

std::shared_ptr<Remote::Message::IncomingMessage>
Server::getValidMessage(IMasterConnection connection, CS101_ASDU asdu) {
  try {
    auto message =
        Remote::Message::IncomingMessage::create(asdu, appLayerParameters);

    // test COT
    if (!message->isValidCauseOfTransmission()) {
      if (message->requireConfirmation()) {
        sendActivationConfirmation(connection, asdu, true);
      }

      onUnexpectedMessage(connection, message, INVALID_COT);

      return {nullptr};
    }

    // test CA
    if (!hasStation(message->getCommonAddress())) {
      if (message->requireConfirmation()) {
        sendActivationConfirmation(connection, asdu, true);
      }

      onUnexpectedMessage(connection, std::move(message), UNKNOWN_CA);
      return {nullptr};
    }

    return std::move(message);
  } catch (const std::exception &e) {
    DEBUG_PRINT(Debug::Server, "get_valid_message] Invalid message format: " +
                                   std::string(e.what()));
  }
  return {nullptr};
}

void Server::rawMessageHandler(void *parameter, IMasterConnection connection,
                               uint_fast8_t *msg, int msgSize, bool sent) {
  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore raw message in shutdown");
    return;
  }

  if (sent) {
    instance->onSendRaw(msg, msgSize);
  } else {
    instance->onReceiveRaw(msg, msgSize);
  }
}

bool Server::interrogationHandler(void *parameter, IMasterConnection connection,
                                  CS101_ASDU asdu,
                                  QualifierOfInterrogation qoi) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Reject interrogation command in shutdown");
    return false;
  }

  if (auto message = instance->getValidMessage(connection, asdu)) {

    // todo support GROUPS
    if (IEC60870_QOI_STATION == qoi) {

      // confirm activation
      instance->sendActivationConfirmation(connection, asdu, false);

      const auto commonAddress = CS101_ASDU_getCA(asdu);
      IEC60870_5_TypeID type = C_TS_TA_1;

      // batch messages per station by type
      std::map<IEC60870_5_TypeID, std::shared_ptr<Remote::Message::Batch>>
          batchMap;

      for (const auto &station : instance->getStations()) {
        if (isGlobalCommonAddress(commonAddress) ||
            station->getCommonAddress() == commonAddress) {

          for (const auto &point : station->getPoints()) {
            type = point->getType();

            // only monitoring points SP,DP,ST,ME,BO
            if (type > M_ME_TF_1 || (type > M_ME_NC_1 && type < M_SP_TB_1))
              continue;

            try {
              // add message to group
              auto g = batchMap.find(type);
              if (g == batchMap.end()) {
                auto b = Remote::Message::Batch::create(
                    CS101_COT_INTERROGATED_BY_STATION);
                b->setOriginatorAddress(message->getOriginatorAddress());
                b->addPoint(point);
                batchMap[type] = std::move(b);
              } else {
                g->second->addPoint(point);
              }
            } catch (const std::exception &e) {
              DEBUG_PRINT(Debug::Server,
                          "Invalid point message for interrogation: " +
                              std::string(e.what()));
            }
          }

          // send batched messages of current station
          for (auto &batch : batchMap) {
            instance->sendBatch(std::move(batch.second), connection);
          }
          batchMap.clear();
        }
      }

      // Notify Master of command finalization
      instance->sendActivationTermination(connection, asdu);
    } else {
      // special groups not supported
      instance->sendActivationConfirmation(connection, asdu, true);

      instance->onUnexpectedMessage(connection, message, UNIMPLEMENTED_GROUP);
    }
  }

  if (debug) {
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "interrogation_handler]"
                          " | IP " +
                              std::string(ipAddrStr) + " | OA " +
                              std::to_string(CS101_ASDU_getOA(asdu)) +
                              " | CA " +
                              std::to_string(CS101_ASDU_getCA(asdu)) +
                              " | TOTAL " + TICTOCNOW(begin));
  }
  return true;
}

bool Server::counterInterrogationHandler(void *parameter,
                                         IMasterConnection connection,
                                         CS101_ASDU asdu, QualifierOfCIC qcc) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server,
                "Reject counter interrogation command in shutdown");
    return false;
  }

  if (auto message = instance->getValidMessage(connection, asdu)) {

    const auto rqt =
        static_cast<CS101_QualifierOfCounterInterrogation>(qcc & 0b00111111);
    const auto frz = static_cast<CS101_FreezeOfCounterInterrogation>(
        (qcc >> 6) & 0b00000011);

    // todo support GROUPS and FREEZE/RESET
    if (rqt == CS101_QualifierOfCounterInterrogation::GENERAL ||
        frz != CS101_FreezeOfCounterInterrogation::READ) {

      // confirm activation
      instance->sendActivationConfirmation(connection, asdu, false);

      const auto commonAddress = CS101_ASDU_getCA(asdu);
      IEC60870_5_TypeID type = C_TS_TA_1;

      // batch messages per station by type
      std::map<IEC60870_5_TypeID, std::shared_ptr<Remote::Message::Batch>>
          batchMap;

      for (const auto &station : instance->getStations()) {
        if (isGlobalCommonAddress(commonAddress) ||
            station->getCommonAddress() == commonAddress) {

          for (const auto &point : station->getPoints()) {
            type = point->getType();
            if (type != M_IT_NA_1 && type != M_IT_TB_1)
              continue;

            try {
              // add message to group
              auto g = batchMap.find(type);
              if (g == batchMap.end()) {
                auto b = Remote::Message::Batch::create(
                    CS101_COT_REQUESTED_BY_GENERAL_COUNTER);
                b->setOriginatorAddress(message->getOriginatorAddress());
                b->addPoint(point);
                batchMap[type] = std::move(b);
              } else {
                g->second->addPoint(point);
              }
            } catch (const std::exception &e) {
              DEBUG_PRINT(Debug::Server,
                          "Invalid point message for counter interrogation: " +
                              std::string(e.what()));
            }
          }

          // send batched messages of current station
          for (auto &batch : batchMap) {
            instance->sendBatch(std::move(batch.second), connection);
          }
          batchMap.clear();
        }
      }

      // Notify Master of command finalization
      instance->sendActivationTermination(connection, asdu);
    } else {
      // special groups not supported
      instance->sendActivationConfirmation(connection, asdu, true);

      instance->onUnexpectedMessage(connection, message, UNIMPLEMENTED_GROUP);
    }
  }

  if (debug) {
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "counter_interrogation_handler]"
                          " | IP " +
                              std::string(ipAddrStr) + " | OA " +
                              std::to_string(CS101_ASDU_getOA(asdu)) +
                              " | CA " +
                              std::to_string(CS101_ASDU_getCA(asdu)) +
                              " | TOTAL " + TICTOCNOW(begin));
  }
  return true;
}

bool Server::readHandler(void *parameter, IMasterConnection connection,
                         CS101_ASDU asdu, int ioAddress) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Reject read command in shutdown");
    return false;
  }

  if (auto message = instance->getValidMessage(connection, asdu)) {

    UnexpectedMessageCause cause = NO_ERROR_CAUSE;
    bool success = false;

    if (auto station = instance->getStation(message->getCommonAddress())) {
      if (auto point = station->getPoint(message->getIOA())) {

        // read not allowed for binary counter / integrated values and
        // protection equipment events
        if (point->getType() < C_SC_NA_1 && point->getType() != M_IT_NA_1 &&
            point->getType() != M_IT_TA_1 && point->getType() != M_IT_TB_1 &&
            point->getType() != M_EP_TA_1 && point->getType() != M_EP_TB_1 &&
            point->getType() != M_EP_TD_1 && point->getType() != M_EP_TE_1 &&
            point->getType() != M_EP_TF_1) {

          // value polling callback
          point->onBeforeRead();
          success = true;

          try {
            // answer the requesting client only, not every connected peer
            instance->transmit(point, CS101_COT_REQUEST,
                               message->getOriginatorAddress(), connection,
                               instance->getConnectionId(connection));
          } catch (const std::exception &e) {
            std::cerr << "[c104.Server] read] Auto respond failed for "
                      << TypeID_toString(point->getType()) << " at IOA "
                      << point->getInformationObjectAddress() << ": "
                      << e.what() << std::endl;
          }

        } else {
          cause = INVALID_TYPE_ID;
        }
      } else {
        cause = UNKNOWN_IOA;
      }
    } else {
      cause = UNKNOWN_CA;
    }

    instance->sendActivationConfirmation(connection, asdu, !success);

    // report error cause
    if (cause != NO_ERROR_CAUSE) {
      instance->onUnexpectedMessage(connection, message, cause);
    }
  }

  if (debug) {
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "read_handler] IOA " + std::to_string(ioAddress) +
                              " | IP " + std::string(ipAddrStr) + " | OA " +
                              std::to_string(CS101_ASDU_getOA(asdu)) +
                              " | CA " +
                              std::to_string(CS101_ASDU_getCA(asdu)) +
                              " | TOTAL " + TICTOCNOW(begin));
  }
  return true;
}

bool Server::asduHandler(void *parameter, IMasterConnection connection,
                         CS101_ASDU asdu) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  // ~Server() must not join this thread, see Module::ScopedThreadOwner.
  // Keyed by parameter and declared before instance, so the marker is
  // still active when the shared_ptr is released at the end of scope.
  Module::ScopedThreadOwner const owner(parameter);
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Reject asdu in shutdown");
    return false;
  }

  // message with more than one object is not allowed for command type ids
  if (auto message = instance->getValidMessage(connection, asdu)) {

    CommandResponseState responseState = RESPONSE_STATE_FAILURE;
    UnexpectedMessageCause cause = NO_ERROR_CAUSE;

    // new clockSyncHandler
    if (message->getType() == C_CS_NA_1) {
      // IncomingMessage rejects zero-object ASDUs, guard anyway
      if (auto info = message->getInfo()) {
        auto time_point =
            info->getRecordedAt().value_or(info->getProcessedAt());

        char ipAddrStr[60];
        IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

        // execute python callback
        responseState =
            instance->onClockSync(std::string(ipAddrStr), time_point);

        DEBUG_PRINT_CONDITION(debug, Debug::Server,
                              "clock_sync_handler] TIME " +
                                  TimePoint_toString(time_point));
      } else {
        DEBUG_PRINT_CONDITION(debug, Debug::Server,
                              "clock_sync_handler] Missing information");
        responseState = RESPONSE_STATE_FAILURE;
      }
    } else {
      if (message->getType() >= C_SC_NA_1) {
        // only an ACTIVATION may select or execute a command
        // a DEACTIVATION may only cancel the requester's own
        // selection and never executes a command
        const auto cot = message->getCauseOfTransmission();
        if (cot != CS101_COT_ACTIVATION && cot != CS101_COT_DEACTIVATION) {
          // reflected confirmations (7, 9, 10) must never execute a command
          responseState = RESPONSE_STATE_NONE;
          cause = UNKNOWN_COT;
        } else if (auto station =
                       instance->getStation(message->getCommonAddress())) {
          if (auto point = station->getPoint(message->getIOA())) {
            if (point->getType() == message->getType()) {

              if (cot == CS101_COT_DEACTIVATION) {
                // cancel only the requester's own matching selection
                // the select/execute flag of the cancel request is
                // ignored and a command is never executed by a DEACTIVATION.
                // A point in DIRECT command mode has no selection, so the
                // request is answered negatively. responseState stays NONE so
                // the generic block below does not reflect a second
                // confirmation: the DEACT_CON is the single answer.
                const bool cancelled =
                    instance->cancelSelection(connection, message);
                instance->sendDeactivationConfirmation(connection, asdu,
                                                       !cancelled);
                responseState = RESPONSE_STATE_NONE;
              } else if (message->isSelectCommand()) {
                if (SELECT_AND_EXECUTE_COMMAND == point->getCommandMode()) {
                  responseState = instance->select(connection, message)
                                      ? RESPONSE_STATE_SUCCESS
                                      : RESPONSE_STATE_FAILURE;
                } else {
                  std::cerr << "[c104.Point] Failed to select point in DIRECT "
                               "command mode"
                            << std::endl;
                  responseState = RESPONSE_STATE_FAILURE;
                }
              } else {
                responseState = instance->execute(connection, message, point);

                if (responseState == RESPONSE_STATE_SUCCESS &&
                    point->getRelatedInformationObjectAutoReturn()) {
                  const auto related_ioa =
                      point->getRelatedInformationObjectAddress();
                  // send related point info in case of auto return
                  if (related_ioa.has_value()) {
                    auto related_point = station->getPoint(related_ioa.value());
                    auto originator = message->getOriginatorAddress();
                    // return the info to the commanding client only; the id
                    // detects a connection object recycled in the meantime
                    const auto connectionId =
                        instance->getConnectionId(connection);
                    std::weak_ptr<Server> weakSelf =
                        instance; // Weak reference to Server
                    std::weak_ptr<Object::DataPoint> weakPoint =
                        related_point; // Weak reference to Point
                    instance->scheduleTask(
                        [weakSelf, weakPoint, originator, connection,
                         connectionId]() {
                          auto self = weakSelf.lock();
                          if (!self)
                            return; // Prevent running if `this` was destroyed

                          auto related = weakPoint.lock();
                          if (!related)
                            return; // Prevent running if `related_point` was
                                    // destroyed

                          try {
                            if (!connectionId.has_value()) {
                              return; // requesting connection unknown
                            }
                            self->transmit(
                                related, CS101_COT_RETURN_INFO_REMOTE,
                                originator, connection, connectionId);
                          } catch (const std::exception &e) {
                            std::cerr
                                << "[c104.Server] asdu_handler] Auto transmit "
                                   "related point failed for "
                                << TypeID_toString(related->getType())
                                << " at IOA "
                                << related->getInformationObjectAddress()
                                << ": " << e.what() << std::endl;
                          }
                        },
                        2);
                  }
                }

              } // else: message->isSelectCommand()

            } else {
              cause = MISMATCHED_TYPE_ID;
            }
          } else {
            cause = UNKNOWN_IOA;
          }
        } else {
          cause = UNKNOWN_CA;
        }
      } else {
        cause = INVALID_TYPE_ID;
      }
    }

    // confirm activation
    if ((responseState != RESPONSE_STATE_NONE) &&
        (message->getCauseOfTransmission() == CS101_COT_ACTIVATION ||
         message->getCauseOfTransmission() == CS101_COT_DEACTIVATION)) {
      instance->sendActivationConfirmation(
          connection, asdu, (responseState == RESPONSE_STATE_FAILURE));
    }

    // report error cause
    if (cause != NO_ERROR_CAUSE) {
      instance->onUnexpectedMessage(connection, message, cause);
    }
  }

  if (debug) {
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "asdu_handler] TYPE " +
            std::string(TypeID_toString(CS101_ASDU_getTypeID(asdu))) +
            " | IP " + std::string(ipAddrStr) + " | OA " +
            std::to_string(CS101_ASDU_getOA(asdu)) + " | CA " +
            std::to_string(CS101_ASDU_getCA(asdu)) + " | TOTAL " +
            TICTOCNOW(begin));
  }
  return true;
}
