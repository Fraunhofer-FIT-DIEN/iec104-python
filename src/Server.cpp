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
#include "remote/TransportSecurity.h"
#include "remote/message/PointCommand.h"
#include "remote/message/PointMessage.h"
#include <pybind11/chrono.h>

using namespace Remote;
using namespace std::chrono_literals;

Server::Server(const std::string &bind_ip, const std::uint_fast16_t tcp_port,
               const std::uint_fast16_t tick_rate_ms,
               const std::uint_fast16_t select_timeout_ms,
               const std::uint_fast8_t max_open_connections,
               std::shared_ptr<Remote::TransportSecurity> transport_security)
    : ip(bind_ip), port(tcp_port), tickRate_ms(tick_rate_ms),
      selectTimeout_ms(select_timeout_ms),
      maxOpenConnections(max_open_connections),
      security(std::move(transport_security)) {
  if (tickRate_ms < 50)
    throw std::range_error("tickRate_ms must be 50 or greater");

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

  // Function pointers for custom handler functions
  CS104_Slave_setConnectionRequestHandler(
      slave, &Server::connectionRequestHandler, this);
  CS104_Slave_setConnectionEventHandler(slave, &Server::connectionEventHandler,
                                        this);
  CS104_Slave_setRawMessageHandler(slave, &Server::rawMessageHandler, this);
  CS104_Slave_setInterrogationHandler(slave, &Server::interrogationHandler,
                                      this);
  CS104_Slave_setCounterInterrogationHandler(
      slave, &Server::counterInterrogationHandler, this);
  // CS104_Slave_setClockSyncHandler(slave, &Server::clockSyncHandler, this);
  CS104_Slave_setReadHandler(slave, &Server::readHandler, this);
  CS104_Slave_setASDUHandler(slave, &Server::asduHandler, this);

  DEBUG_PRINT(Debug::Server, "Created");
}

Server::~Server() {
  // stops and destroys the slave
  stop();
  CS104_Slave_destroy(slave);

  {
    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    stations.clear();
  }

  DEBUG_PRINT(Debug::Server, "Removed");
}

std::string Server::getIP() const { return ip; }

std::uint_fast16_t Server::getPort() const { return port; }

void Server::setMaxOpenConnections(std::uint_fast8_t max_open_connections) {
  uint_fast8_t const prev = maxOpenConnections.load();
  if (prev != max_open_connections) {
    maxOpenConnections.store(max_open_connections);
    CS104_Slave_setMaxOpenConnections(slave, max_open_connections);
  }
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
  schedulePeriodicTask([this]() { sendInventory(CS101_COT_PERIODIC); },
                       tickRate_ms);
  schedulePeriodicTask(
      [this]() {
        cleanupSelections();
        scheduleDataPointTimer();
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
        scheduleTask([point]() { point->onTimer(); }, counter++);
      }
    }
  }
}

void Server::schedulePeriodicTask(const std::function<void()> &task,
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

void Server::scheduleTask(const std::function<void()> &task, int delay) {
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
  bool const debug = DEBUG_TEST(Debug::Server);
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
              debug, Debug::Server,
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
      std::cerr << "[c104.Server] loop] Task aborted: " << e.what()
                << std::endl;
    }
  }
  if (!tasks.empty()) {
    DEBUG_PRINT_CONDITION(debug, Debug::Server,
                          "loop] Tasks dropped due to stop: " +
                              std::to_string(tasks.size()));
    std::priority_queue<Task> empty;
    std::swap(tasks, empty);
  }
  running.store(false);
}

void Server::stop() {
  Module::ScopedGilRelease const scoped("Server.stop");
  bool expected = true;
  if (!enabled.compare_exchange_strong(expected, false)) {
    DEBUG_PRINT(Debug::Server, "stop] Already stopped");
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

  CS104_Slave_stop(slave);

  connectionMap.clear();
  activeConnections.store(0);
  openConnections.store(0);

  DEBUG_PRINT(Debug::Server, "stop] Stopped");
}

bool Server::isRunning() { return enabled && CS104_Slave_isRunning(slave); }

bool Server::hasStations() const {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  return !stations.empty();
}

bool Server::isExistingConnection(IMasterConnection connection) {
  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);

  auto it = connectionMap.find(connection);
  return it != connectionMap.end();
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

void Server::cleanupSelections() {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  selectionVector.erase(
      std::remove_if(selectionVector.begin(), selectionVector.end(),
                     [this, now](const Selection &s) {
                       if ((now - s.created) < selectTimeout_ms) {
                         return false;
                       } else {
                         unselect(s);
                         return true;
                       }
                     }),
      selectionVector.end());
}

void Server::cleanupSelections(IMasterConnection connection) {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  selectionVector.erase(std::remove_if(selectionVector.begin(),
                                       selectionVector.end(),
                                       [this, connection](const Selection &s) {
                                         if (s.connection == connection) {
                                           unselect(s);
                                           return true;
                                         } else {
                                           return false;
                                         }
                                       }),
                        selectionVector.end());
}

void Server::cleanupSelection(uint16_t ca, uint32_t ioa) {
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  selectionVector.erase(std::remove_if(selectionVector.begin(),
                                       selectionVector.end(),
                                       [this, ca, ioa](const Selection &s) {
                                         if (s.ca == ca && s.ioa == ioa) {
                                           unselect(s);
                                           return true;
                                         } else {
                                           return false;
                                         }
                                       }),
                        selectionVector.end());
}

std::optional<uint8_t> Server::getSelector(const uint16_t ca,
                                           const uint32_t ioa) {
  auto now = std::chrono::steady_clock::now();
  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it = std::find_if(
      selectionVector.begin(), selectionVector.end(),
      [ca, ioa](const Selection &s) { return s.ca == ca && s.ioa == ioa; });

  // selection NOT found
  if (it != selectionVector.end() && (now - it->created) < selectTimeout_ms) {
    return it->oa;
  }
  return std::nullopt;
}

bool Server::select(IMasterConnection connection,
                    std::shared_ptr<Remote::Message::IncomingMessage> message) {
  const auto type = message->getType();
  if (type < C_SC_NA_1 || C_BO_NA_1 == type || C_SE_TC_1 < type) {
    throw std::invalid_argument(
        "Only control points, except for binary commands can be selected");
  }

  const uint8_t oa = message->getOriginatorAddress();
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();
  auto now = std::chrono::steady_clock::now();

  std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
  auto it = std::find_if(
      selectionVector.begin(), selectionVector.end(),
      [ca, ioa](const Selection &s) { return s.ca == ca && s.ioa == ioa; });

  // selection NOT found
  if (it == selectionVector.end()) {
    CS101_ASDU cp = CS101_ASDU_clone(message->getAsdu(), nullptr);
    selectionVector.emplace_back<Selection>({cp, oa, ca, ioa, connection, now});
  }
  // selection found
  else {
    if ((it->connection != connection) &&
        (now - it->created) < selectTimeout_ms) {
      return false;
    } else {
      it->created = now;
    }
  }

  return true;
}

void Server::unselect(const Selection &selection) {
  scheduleTask([this, selection]() {
    sendActivationTermination(selection.connection, selection.asdu);
    CS101_ASDU_destroy(selection.asdu);
  });
}

CommandResponseState
Server::execute(IMasterConnection connection,
                std::shared_ptr<Remote::Message::IncomingMessage> message,
                std::shared_ptr<Object::DataPoint> point) {
  bool selected = false;
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();

  if (SELECT_AND_EXECUTE_COMMAND == point->getCommandMode()) {
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<Module::GilAwareMutex> const lock(selection_mutex);
    auto it = std::find_if(
        selectionVector.begin(), selectionVector.end(),
        [ca, ioa](const Selection &s) { return s.ca == ca && s.ioa == ioa; });

    // selection NOT found
    selected = (it != selectionVector.end() && (it->connection == connection) &&
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
    scheduleTask([this, ca, ioa]() { cleanupSelection(ca, ioa); }, 1);
  }

  return res;
}

void Server::setOnReceiveRawCallback(py::object &callable) {
  py_onReceiveRaw.reset(callable);
}

void Server::onReceiveRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onReceiveRaw.is_set()) {
    // create a copy
    auto *cp = new char[msgSize];
    memcpy(cp, msg, msgSize);

    scheduleTask([this, cp, msgSize]() {
      DEBUG_PRINT(Debug::Server, "CALLBACK on_receive_raw");
      Module::ScopedGilAcquire const scoped("Server.on_receive_raw");
      PyObject *pymemview =
          PyMemoryView_FromMemory((char *)cp, msgSize, PyBUF_READ);
      PyObject *pybytes = PyBytes_FromObject(pymemview);

      py_onReceiveRaw.call(shared_from_this(), py::handle(pybytes));

      delete[] cp;
    });
  }
}

void Server::setOnSendRawCallback(py::object &callable) {
  py_onSendRaw.reset(callable);
}

void Server::onSendRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onSendRaw.is_set()) {
    // create a copy
    auto *cp = new char[msgSize];
    memcpy(cp, msg, msgSize);

    scheduleTask([this, cp, msgSize]() {
      DEBUG_PRINT(Debug::Server, "CALLBACK on_send_raw");
      Module::ScopedGilAcquire const scoped("Server.on_send_raw");
      PyObject *pymemview =
          PyMemoryView_FromMemory((char *)cp, msgSize, PyBUF_READ);
      PyObject *pybytes = PyBytes_FromObject(pymemview);

      py_onSendRaw.call(shared_from_this(), py::handle(pybytes));

      delete[] cp;
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

    std::tm localtime = *std::localtime(&tt);

    PyObject *pydate = PyDateTime_FromDateAndTime(
        localtime.tm_year + 1900, localtime.tm_mon + 1, localtime.tm_mday,
        localtime.tm_hour, localtime.tm_min, localtime.tm_sec, us.count());

    if (py_onClockSync.call(shared_from_this(), _ip, py::handle(pydate))) {
      try {
        return py_onClockSync.getResult();
      } catch (const std::exception &e) {
        DEBUG_PRINT(Debug::Server, "on_clock_sync] Invalid callback result: " +
                                       std::string(e.what()));
        return RESPONSE_STATE_FAILURE;
      }
    }
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
  switch (cause) {
  case INVALID_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Invalid type id");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_TYPE_ID);
    IMasterConnection_sendASDU(connection, asdu);
    break;
  case MISMATCHED_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Mismatching type id");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_TYPE_ID);
    IMasterConnection_sendASDU(connection, asdu);
    break;
  case UNKNOWN_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unknown type id");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_TYPE_ID);
    IMasterConnection_sendASDU(connection, asdu);
    break;
  case INVALID_COT:
  case UNKNOWN_COT:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Invalid COT");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_COT);
    IMasterConnection_sendASDU(connection, asdu);
    break;
  case UNKNOWN_CA:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unknown CA");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_CA);
    IMasterConnection_sendASDU(connection, asdu);
    break;
  case UNKNOWN_IOA:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unknown IOA");
    CS101_ASDU_setCOT(asdu, CS101_COT_UNKNOWN_IOA);
    IMasterConnection_sendASDU(connection, asdu);
    break;
  case UNIMPLEMENTED_GROUP:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Unimplemented group");
    break;
  default: {
  }
  }

  if (py_onUnexpectedMessage.is_set()) {
    scheduleTask([this, message, cause]() {
      DEBUG_PRINT(Debug::Server, "CALLBACK on_unexpected_message");
      Module::ScopedGilAcquire const scoped("Server.on_unexpected_message");
      py_onUnexpectedMessage.call(shared_from_this(), message, cause);
    });
  }
}

void Server::setOnConnectCallback(py::object &callable) {
  py_onConnect.reset(callable);
}

std::uint_fast16_t Server::getTickRate_ms() const { return tickRate_ms; }

bool Server::connectionRequestHandler(void *parameter, const char *ipAddress) {
  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Server, "Reject connection request in shutdown");
    return false;
  }

  if (instance->py_onConnect.is_set()) {
    DEBUG_PRINT(Debug::Server, "CALLBACK on_connect");
    Module::ScopedGilAcquire const scoped("Server.on_connect");
    std::string const ip(ipAddress);
    if (instance->py_onConnect.call(instance, ip)) {
      try {
        return instance->py_onConnect.getResult();
      } catch (const std::exception &e) {
        DEBUG_PRINT(Debug::Server, "on_connect] Invalid callback result: " +
                                       std::string(e.what()));
        return false;
      }
    }
  }

  return true;
}

void Server::connectionEventHandler(void *parameter,
                                    IMasterConnection connection,
                                    CS104_PeerConnectionEvent event) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Server, "Ignore connection event " +
                                   PeerConnectionEvent_toString(event) +
                                   " in shutdown");
    return;
  }

  char ipAddrStr[60];
  IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

  {
    std::lock_guard<Module::GilAwareMutex> const lock(
        instance->connection_mutex);

    if (event == CS104_CON_EVENT_CONNECTION_OPENED) {
      // set as invalid receiver
      auto it = instance->connectionMap.find(connection);
      if (it == instance->connectionMap.end()) {
        instance->connectionMap[connection] = false;
      } else if (it->second) {
        it->second = false;
        instance->activeConnections--;
      }

    } else if (event == CS104_CON_EVENT_CONNECTION_CLOSED) {
      // set as invalid receiver
      auto it = instance->connectionMap.find(connection);
      if (it != instance->connectionMap.end()) {
        if (it->second) {
          instance->activeConnections--;
        }
        instance->connectionMap.erase(it);
      }
      // remove selections
      instance->scheduleTask([instance, connection]() {
        instance->cleanupSelections(connection);
      });
    } else if (event == CS104_CON_EVENT_ACTIVATED) {
      // set as valid receiver
      auto it = instance->connectionMap.find(connection);
      if (it == instance->connectionMap.end()) {
        instance->connectionMap[connection] = true;
        instance->activeConnections++;
      } else {
        if (!it->second) {
          instance->activeConnections++;
        }
        it->second = true;
      }

    } else if (event == CS104_CON_EVENT_DEACTIVATED) {
      // set as invalid receiver
      auto it = instance->connectionMap.find(connection);
      if (it == instance->connectionMap.end()) {
        instance->connectionMap[connection] = false;
      } else if (it->second) {
        instance->activeConnections--;
        it->second = false;
      }
    }
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "connection_event_handler] Connection " +
                              PeerConnectionEvent_toString(event) + " by " +
                              std::string(ipAddrStr) + " | TOTAL " +
                              TICTOC(begin, end));
  }
}

bool Server::transmit(std::shared_ptr<Object::DataPoint> point,
                      const CS101_CauseOfTransmission cause) {
  auto type = point->getType();

  // report monitoring point
  if (type < S_IT_TC_1) {
    auto message = Message::PointMessage::create(std::move(point));
    message->setCauseOfTransmission(cause);
    return send(std::move(message));
  }

  // lazy confirm previous control command
  if (type > S_IT_TC_1 && type < M_EI_NA_1) {
    auto message = Message::PointCommand::create(std::move(point));
    message->setCauseOfTransmission(cause);
    return send(std::move(message));
  }

  throw std::invalid_argument("Invalid point type");
}

bool Server::send(std::shared_ptr<Remote::Message::OutgoingMessage> message,
                  IMasterConnection connection) {
  if (!enabled.load() || !hasActiveConnections())
    return false;

  Module::ScopedGilRelease const scoped("Server.send");

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  if (connection) {
    auto param = IMasterConnection_getApplicationLayerParameters(connection);
    message->setOriginatorAddress(param->originatorAddress);
  }

  CS101_ASDU asdu = CS101_ASDU_create(
      appLayerParameters, message->isSequence(),
      message->getCauseOfTransmission(), message->getOriginatorAddress(),
      message->getCommonAddress(), message->isTest(), message->isNegative());

  // @todo add support for packed messages / multiple IOs in outgoing message
  CS101_ASDU_addInformationObject(asdu, message->getInformationObject());

  // @todo high vs low priority messages
  if (!connection || CS101_COT_PERIODIC == message->getCauseOfTransmission() ||
      CS101_COT_SPONTANEOUS == message->getCauseOfTransmission())
    // low priority
    CS104_Slave_enqueueASDU(slave, asdu);
  else {
    // high priority
    IMasterConnection_sendASDU(connection, asdu);
  }

  CS101_ASDU_destroy(asdu);

  if (debug) {
    end = std::chrono::steady_clock::now();
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

void Server::sendActivationConfirmation(IMasterConnection connection,
                                        CS101_ASDU asdu, bool negative) {
  if (!isExistingConnection(connection))
    return;

  CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_CON);
  CS101_ASDU_setNegative(asdu, negative);

  if (isGlobalCommonAddress(CS101_ASDU_getCA(asdu))) {
    DEBUG_PRINT(Debug::Server, "send_activation_confirmation] to all MTUs");
    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    for (auto &s : stations) {
      CS101_ASDU_setCA(asdu, s->getCommonAddress());
      IMasterConnection_sendASDU(connection, asdu);
    }
  } else {
    DEBUG_PRINT(Debug::Server,
                "send_activation_confirmation] to requesting MTU");
    IMasterConnection_sendASDU(connection, asdu);
  }
}

void Server::sendActivationTermination(IMasterConnection connection,
                                       CS101_ASDU asdu) {
  if (!isExistingConnection(connection))
    return;

  CS101_ASDU_setCOT(asdu, CS101_COT_ACTIVATION_TERMINATION);

  if (isGlobalCommonAddress(CS101_ASDU_getCA(asdu))) {

    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    for (auto &s : stations) {
      CS101_ASDU_setCA(asdu, s->getCommonAddress());
      IMasterConnection_sendASDU(connection, asdu);
    }
  } else {
    IMasterConnection_sendASDU(connection, asdu);
  }
}

void Server::sendInventory(const CS101_CauseOfTransmission cot,
                           const uint_fast16_t commonAddress,
                           IMasterConnection connection) {
  if (!enabled.load() || !hasActiveConnections())
    return;

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;

  // always initialize begin, as it is used for cyclic report interval
  begin = std::chrono::steady_clock::now();

  bool empty = true;
  IEC60870_5_TypeID type = C_TS_TA_1;

  for (const auto &station : getStations()) {
    if (isGlobalCommonAddress(commonAddress) ||
        station->getCommonAddress() == commonAddress) {

      // group messages per station by type
      std::map<IEC60870_5_TypeID,
               std::map<uint_fast16_t,
                        std::shared_ptr<Remote::Message::OutgoingMessage>>>
          pointGroup;

      for (const auto &point : station->getPoints()) {
        type = point->getType();

        // only monitoring points
        if (type > 41)
          continue;

        // full interrogation contain all monitoring data
        // Update all (!) data points before transmitting them
        if (CS101_COT_PERIODIC == cot) {
          // is ready for cyclic transmission ?
          const auto next = point->nextReportAt();
          // disabled cyclic report
          if (!next.has_value() || begin < next.value()) {
            continue;
          }

          // value polling callback
          point->onBeforeAutoTransmit();
        } else {
          point->onBeforeRead();
        }

        try {
          // transmit value to client
          auto message = Remote::Message::PointMessage::create(point);
          message->setCauseOfTransmission(cot);
          // message->send(connection);

          // add message to group
          auto g = pointGroup.find(type);
          if (g == pointGroup.end()) {
            pointGroup[type] =
                std::map<uint_fast16_t,
                         std::shared_ptr<Remote::Message::OutgoingMessage>>{
                    {point->getInformationObjectAddress(), message}};
          } else {
            g->second[point->getInformationObjectAddress()] = message;
          }
        } catch (const std::exception &e) {
          DEBUG_PRINT(Debug::Server, "Invalid point message for inventory: " +
                                         std::string(e.what()));
        }
      }

      // send grouped messages of current station
      for (auto &group : pointGroup) {
        empty = false;
        bool const isSequence = false;
        bool const isTest = false;
        bool const isNegative = false;

        /// indicator if an InformationObject was added to ASDU or not
        bool added = false;

        CS101_ASDU asdu =
            CS101_ASDU_create(appLayerParameters, isSequence, cot, 0,
                              station->getCommonAddress(), isTest, isNegative);
        // std::stringstream c;
        // c << "GRP-" << TypeID_toString(group.first) << ": ";
        for (auto &message : group.second) {
          // c << std::to_string(message.second->getInformationObjectAddress())
          // << ",";
          added = CS101_ASDU_addInformationObject(
              asdu, message.second->getInformationObject());

          // not added => ASDU packet size exceeded => send asdu and create a
          // new one
          if (!added) {
            // send asdu
            if (connection) {
              IMasterConnection_sendASDU(connection, asdu);
            } else {
              CS104_Slave_enqueueASDU(slave, asdu);
            }

            // recreate new asdu
            CS101_ASDU_destroy(asdu);
            asdu = CS101_ASDU_create(appLayerParameters, isSequence, cot, 0,
                                     station->getCommonAddress(), isTest,
                                     isNegative);

            // add message to new asdu
            added = CS101_ASDU_addInformationObject(
                asdu, message.second->getInformationObject());
            if (!added) {
              DEBUG_PRINT(Debug::Server,
                          "Dropped message for inventory, "
                          "cannot be added to new asdu: " +
                              std::to_string(message.second->getIOA()));
            }
          }
        }
        // std::cout << c.str() << std::endl;

        // if ASDU is not empty, send ASDU
        if (added) {
          if (connection) {
            IMasterConnection_sendASDU(connection, asdu);
          } else {
            CS104_Slave_enqueueASDU(slave, asdu);
          }
        }

        // @todo what about ASDU destruction ?!
        // high priority, ASDU gets destroyed automatically
        // low priority, destroy ASDU manually
        CS101_ASDU_destroy(asdu);

        // free messages
        for (auto &message : group.second) {
          message.second.reset();
        }
      }
    }
  }

  if (debug) {
    end = std::chrono::steady_clock::now();

    if (CS101_COT_PERIODIC == cot) {
      if (!empty) {

        DEBUG_PRINT_CONDITION(true, Debug::Server,
                              "auto_transmit] TOTAL " + TICTOC(begin, end));
      }
    } else {
      DEBUG_PRINT_CONDITION(true, Debug::Server,
                            "interrogation_response] TOTAL " +
                                TICTOC(begin, end));
    }
  }
}

std::shared_ptr<Remote::Message::IncomingMessage>
Server::getValidMessage(IMasterConnection connection, CS101_ASDU asdu) {
  try {
    auto message =
        Remote::Message::IncomingMessage::create(asdu, appLayerParameters);

    // @todo enabled advanced COT check
    /*
    if (!message->isValidCauseOfTransmission()) {
        DEBUG_PRINT(Debug::Server, "Server.getValidMessage] Unknown cause of
    transmission " +
    std::string(CS101_CauseOfTransmission_toString(message->getCauseOfTransmission())));

        if (message->requireConfirmation()) {
            sendActivationConfirmation(connection, asdu, true);
        }

        onUnexpectedMessage(connection, message.get(), INVALID_COT);

        return {nullptr};
    }*/

    // test & message ip/ca mismatch
    if (!hasStation(message->getCommonAddress())) {

      DEBUG_PRINT(Debug::Server,
                  "get_valid_message] Unknown commonAddress " +
                      std::to_string(message->getCommonAddress()));

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
  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Server, "Ignore raw message in shutdown");
    return;
  }

  if (sent) {
    instance->onSendRaw(msg, msgSize);
  } else {
    instance->onReceiveRaw(msg, msgSize);
  }
}

/*
bool Server::clockSyncHandler(void *parameter, IMasterConnection connection,
CS101_ASDU asdu, CP56Time2a newtime) { bool debug = DEBUG_TEST(Debug::Server);
    std::chrono::steady_clock::time_point begin, end;
    if (debug) {
        begin = std::chrono::steady_clock::now();
    }

    std::shared_ptr<Server> instance{};

    try {
        instance = static_cast<Server *>(parameter)->shared_from_this();
    } catch (const std::bad_weak_ptr &e) {
        DEBUG_PRINT(Debug::Server, "Reject clock sync command in shutdown");
        return false;
    }

    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    if (auto message = instance->getValidMessage(connection, asdu)) {
        // execute python callback
        instance->onClockSync(std::string(ipAddrStr), newtime);
    }

    if (debug) {
        end = std::chrono::steady_clock::now();

        DEBUG_PRINT_CONDITION(true, Debug::Server,
                              "Server.clockSyncHandler] TIME " +
CP56Time2a_toString(newtime) + " | IP " + std::string(ipAddrStr) + " | OA " +
std::to_string(CS101_ASDU_getOA(asdu)) + " | CA " +
std::to_string(CS101_ASDU_getCA(asdu)) + " | TOTAL " + TICTOC(begin, end));
    }
    return true;
}*/

bool Server::interrogationHandler(void *parameter, IMasterConnection connection,
                                  CS101_ASDU asdu,
                                  QualifierOfInterrogation qoi) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Server, "Reject interrogation command in shutdown");
    return false;
  }

  if (auto message = instance->getValidMessage(connection, asdu)) {

    // all data ^= INTERROGATED_BY_STATION, special groups via other qoi
    if (IEC60870_QOI_STATION == qoi) {

      // confirm activation
      instance->sendActivationConfirmation(connection, asdu, false);

      // send all available information
      instance->sendInventory((CS101_CauseOfTransmission)qoi,
                              message->getCommonAddress(), connection);

      // Notify Master of command finalization
      instance->sendActivationTermination(connection, asdu);
    } else {
      // special groups not supported
      instance->sendActivationConfirmation(connection, asdu, true);

      instance->onUnexpectedMessage(connection, message, UNIMPLEMENTED_GROUP);
    }
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "interrogation_handler]"
                          " | IP " +
                              std::string(ipAddrStr) + " | OA " +
                              std::to_string(CS101_ASDU_getOA(asdu)) +
                              " | CA " +
                              std::to_string(CS101_ASDU_getCA(asdu)) +
                              " | TOTAL " + TICTOC(begin, end));
  }
  return true;
}

bool Server::counterInterrogationHandler(void *parameter,
                                         IMasterConnection connection,
                                         CS101_ASDU asdu, QualifierOfCIC qcc) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Server,
                "Reject counter interrogation command in shutdown");
    return false;
  }

  if (auto message = instance->getValidMessage(connection, asdu)) {

    // all data ^= REQUESTED_BY_GENERAL_COUNTER, special groups via other qcc
    if (IEC60870_QCC_RQT_GENERAL == qcc) {

      // confirm activation
      instance->sendActivationConfirmation(connection, asdu, false);

      // send all available information
      //@todo implement handler -> norm 7.2.6
      // M_IT_ .... messages

      // Notify Master of command finalization
      instance->sendActivationTermination(connection, asdu);
    } else {
      // special groups not supported
      instance->sendActivationConfirmation(connection, asdu, true);

      instance->onUnexpectedMessage(connection, message, UNIMPLEMENTED_GROUP);
    }
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "counter_interrogation_handler]"
                          " | IP " +
                              std::string(ipAddrStr) + " | OA " +
                              std::to_string(CS101_ASDU_getOA(asdu)) +
                              " | CA " +
                              std::to_string(CS101_ASDU_getCA(asdu)) +
                              " | TOTAL " + TICTOC(begin, end));
  }
  return true;
}

bool Server::readHandler(void *parameter, IMasterConnection connection,
                         CS101_ASDU asdu, int ioAddress) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
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
            instance->transmit(point, CS101_COT_REQUEST);
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
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "read_handler] IOA " + std::to_string(ioAddress) +
                              " | IP " + std::string(ipAddrStr) + " | OA " +
                              std::to_string(CS101_ASDU_getOA(asdu)) +
                              " | CA " +
                              std::to_string(CS101_ASDU_getCA(asdu)) +
                              " | TOTAL " + TICTOC(begin, end));
  }
  return true;
}

bool Server::asduHandler(void *parameter, IMasterConnection connection,
                         CS101_ASDU asdu) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  std::shared_ptr<Server> instance{};

  try {
    instance = static_cast<Server *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Server, "Reject asdu in shutdown");
    return false;
  }

  // message with more than one object is not allowed for command type ids
  if (auto message = instance->getValidMessage(connection, asdu)) {

    CommandResponseState responseState = RESPONSE_STATE_FAILURE;
    UnexpectedMessageCause cause = NO_ERROR_CAUSE;

    // new clockSyncHandler
    if (message->getType() == C_CS_NA_1) {
      auto info = message->getInfo();
      auto time_point = info->getRecordedAt().value_or(info->getProcessedAt());

      char ipAddrStr[60];
      IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

      // execute python callback
      responseState = instance->onClockSync(std::string(ipAddrStr), time_point);

      DEBUG_PRINT_CONDITION(debug, Debug::Server,
                            "clock_sync_handler] TIME " +
                                TimePoint_toString(time_point));
    } else {
      if (message->getType() >= C_SC_NA_1) {
        if (auto station = instance->getStation(message->getCommonAddress())) {
          if (auto point = station->getPoint(message->getIOA())) {
            if (point->getType() == message->getType()) {

              if (message->isSelectCommand()) {
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
                    instance->scheduleTask(
                        [instance, related_point]() {
                          try {
                            instance->transmit(related_point,
                                               CS101_COT_RETURN_INFO_REMOTE);
                          } catch (const std::exception &e) {
                            std::cerr
                                << "[c104.Server] asdu_handler] Auto transmit "
                                   "related point failed for "
                                << TypeID_toString(related_point->getType())
                                << " at IOA "
                                << related_point->getInformationObjectAddress()
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
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "asdu_handler] TYPE " +
            std::string(TypeID_toString(CS101_ASDU_getTypeID(asdu))) +
            " | IP " + std::string(ipAddrStr) + " | OA " +
            std::to_string(CS101_ASDU_getOA(asdu)) + " | CA " +
            std::to_string(CS101_ASDU_getCA(asdu)) + " | TOTAL " +
            TICTOC(begin, end));
  }
  return true;
}
