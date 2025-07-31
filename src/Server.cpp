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
#include "object/DataPoint.h"
#include "object/Station.h"
#include "object/information/ICommand.h"
#include "object/information/IntegratedTotalInfo.h"
#include "remote/TransportSecurity.h"
#include "remote/message/Batch.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/InvalidMessageException.h"
#include "remote/message/OutgoingMessage.h"
#include "transformer/Type.h"

#include <pybind11/chrono.h>
#include <sstream>

using namespace Remote;
using namespace std::chrono_literals;

// Define static members
std::unordered_map<void *, std::weak_ptr<Server>> Server::instanceMap;
std::mutex Server::instanceMapMutex;

std::shared_ptr<Server>
Server::create(const std::string &bind_ip, const uint_fast16_t tcp_port,
               const uint_fast16_t tick_rate_ms,
               const uint_fast16_t select_timeout_ms,
               const std::uint_fast8_t max_open_connections,
               std::shared_ptr<Remote::TransportSecurity> transport_security) {
  Module::ScopedGilRelease const scoped("Server.create");

  // Not using std::make_shared because the constructor is private.
  auto server = std::shared_ptr<Server>(
      new Server(bind_ip, tcp_port, tick_rate_ms, select_timeout_ms,
                 max_open_connections, std::move(transport_security)));

  // track reference as a weak pointer for safe static callbacks
  void *key = static_cast<void *>(server.get());
  std::lock_guard<std::mutex> lock(instanceMapMutex);
  instanceMap[key] = server;

  return server;
}

Server::Server(const std::string &bind_ip, const std::uint_fast16_t tcp_port,
               const std::uint_fast16_t tick_rate_ms,
               const std::uint_fast16_t select_timeout_ms,
               const std::uint_fast8_t max_open_connections,
               std::shared_ptr<Remote::TransportSecurity> transport_security)
    : ip(bind_ip), port(tcp_port), tickRate_ms(tick_rate_ms),
      selectionManager(select_timeout_ms),
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

  // use lib60870-C defaults for connection timeouts
  // auto param = CS104_Slave_getConnectionParameters(slave);
  // param->t0 = 10; // socket connect timeout
  // param->t1 = 15; // acknowledgement timeout
  // param->t2 = 10; // acknowledgement interval

  // Function pointers for custom handler functions
  void *key = static_cast<void *>(this);
  CS104_Slave_setConnectionRequestHandler(
      slave, &Server::connectionRequestHandler, key);
  CS104_Slave_setConnectionEventHandler(slave, &Server::connectionEventHandler,
                                        key);
  CS104_Slave_setRawMessageHandler(slave, &Server::rawMessageHandler, key);
  CS104_Slave_setInterrogationHandler(slave, &Server::interrogationHandler,
                                      key);
  CS104_Slave_setCounterInterrogationHandler(
      slave, &Server::counterInterrogationHandler, key);
  CS104_Slave_setClockSyncHandler(slave, &Server::clockSyncHandler, key);
  CS104_Slave_setReadHandler(slave, &Server::readHandler, key);
  CS104_Slave_setASDUHandler(slave, &Server::asduHandler, key);

  DEBUG_PRINT(Debug::Server, "Created");
}

Server::~Server() {
  // stops and destroys the slave
  stop();

  Module::ScopedGilRelease const scoped("Server.destroy");
  {
    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    stations.clear();
  }
  {
    std::lock_guard<std::mutex> lock(instanceMapMutex);
    instanceMap.erase(
        static_cast<void *>(this)); // Remove from map on destruction
  }

  CS104_Slave_destroy(slave);

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
    throw std::runtime_error("Can't start server: Port in use or IP invalid?");
  }

  executor = std::make_shared<Tasks::Executor>();

  DEBUG_PRINT(Debug::Server, "start] Started");

  // Schedule periodics based on tickRate
  std::weak_ptr<Server> weakSelf =
      shared_from_this(); // Weak reference to `this`
  executor->addPeriodic(SAFE_TASK(sendPeriodicInventory), tickRate_ms);
  executor->addPeriodic(SAFE_TASK(selectionManager.cleanup), tickRate_ms);
  executor->addPeriodic(SAFE_TASK(scheduleDataPointTimer), tickRate_ms);
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

        std::weak_ptr<Object::DataPoint> weakSelf =
            point; // Weak reference to `point`
        executor->add(SAFE_TASK(onTimer), counter++);
      }
    }
  }
}

void Server::stop() {
  Module::ScopedGilRelease const scoped("Server.stop");
  bool expected = true;
  if (!enabled.compare_exchange_strong(expected, false)) {
    DEBUG_PRINT(Debug::Server, "stop] Already stopped");
    return;
  }

  executor->stop();

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

std::vector<std::shared_ptr<Object::Station>> Server::getStations() const {
  std::lock_guard<Module::GilAwareMutex> const lock(station_mutex);

  return stations;
}

std::shared_ptr<Object::Station>
Server::getStation(const std::uint_fast16_t commonAddress) const {
  if (IEC60870_GLOBAL_COMMON_ADDRESS == commonAddress) {
    return {nullptr};
  }

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

  stations.erase(
      std::remove_if(
          stations.begin(), stations.end(),
          [commonAddress](const std::shared_ptr<Object::Station> &station) {
            // Check if the current DataPoint matches the provided address
            if (station->getCommonAddress() == commonAddress) {
              station->detach();
              return true;
            }
            return false;
          }),
      stations.end());

  return (stations.size() < originalSize); // Success if the size decreased
}

void Server::select(std::shared_ptr<Remote::Message::IncomingMessage> message,
                    std::shared_ptr<Object::DataPoint> point) {
  if (SELECT_AND_EXECUTE_COMMAND != point->getCommandMode()) {
    throw Remote::Message::InvalidMessageException(message,
                                                   CUSTOM_NOT_SELECTABLE);
  }

  const uint8_t oa = message->getOriginatorAddress();
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();
  auto now = std::chrono::steady_clock::now();

  if (!selectionManager.add({oa, ca, ioa, now})) {
    throw Remote::Message::InvalidMessageException(message,
                                                   CUSTOM_ALREADY_SELECTED);
  }
}

CommandResponseState
Server::execute(std::shared_ptr<Remote::Message::IncomingMessage> message,
                std::shared_ptr<Object::DataPoint> point) {
  bool selected = false;
  const uint8_t oa = message->getOriginatorAddress();
  const uint16_t ca = message->getCommonAddress();
  const uint32_t ioa = message->getIOA();
  auto now = std::chrono::steady_clock::now();

  if (SELECT_AND_EXECUTE_COMMAND == point->getCommandMode()) {
    selected = selectionManager.exists({oa, ca, ioa, now});
    if (!selected) {
      throw Remote::Message::InvalidMessageException(message,
                                                     CUSTOM_NOT_SELECTED);
    }
  }

  // execute python callback
  auto res = point->onReceive(std::move(message));

  // remove selection
  if (selected) {
    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    executor->add(SAFE_TASK_CAPTURE(selectionManager.remove, ca, ioa), 1);
  }

  if (res == RESPONSE_STATE_SUCCESS) {
    // handle auto-return feature
    const auto auto_return = point->getRelatedInformationObjectAutoReturn();
    const auto related_ioa = point->getRelatedInformationObjectAddress();

    // send related point info in case of auto return
    if (auto_return && related_ioa.has_value()) {
      const auto station = point->getStation();
      auto related_point =
          station ? station->getPoint(related_ioa.value()) : nullptr;

      std::weak_ptr<Server> weakSelf =
          shared_from_this(); // Weak reference to Server
      std::weak_ptr<Object::DataPoint> weakPoint =
          related_point; // Weak reference to Point

      executor->add(
          SAFE_LAMBDA_CAPTURE(
              {
                auto related = weakPoint.lock();
                if (!related)
                  return;

                try {
                  self->transmit(related, CS101_COT_RETURN_INFO_REMOTE);
                } catch (const std::exception &e) {
                  std::cerr << "[c104.Server] asdu_handler] Auto transmit "
                               "related point failed for "
                            << related->getInfo()->name() << " at IOA "
                            << related->getInformationObjectAddress() << ": "
                            << e.what() << std::endl;
                }
              },
              weakPoint),
          2);
    }
  }

  return res;
}

CS104_APCIParameters Server::getParameters() const {
  return CS104_Slave_getConnectionParameters(slave);
}

void Server::setOnReceiveRawCallback(py::object &callable) {
  py_onReceiveRaw.reset(callable);
}

void Server::onReceiveRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onReceiveRaw.is_set()) {
    // create a copy
    auto cp = std::shared_ptr<char[]>(new char[msgSize]);
    memcpy(cp.get(), msg, msgSize);

    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    executor->add(SAFE_LAMBDA_CAPTURE(
        {
          DEBUG_PRINT(Debug::Server, "CALLBACK on_receive_raw");
          Module::ScopedGilAcquire const scoped("Server.on_receive_raw");
          PyObject *pymemview =
              PyMemoryView_FromMemory(cp.get(), msgSize, PyBUF_READ);
          PyObject *pybytes = PyBytes_FromObject(pymemview);

          self->py_onReceiveRaw.call(self, py::handle(pybytes));
        },
        cp, msgSize));
  }
}

void Server::setOnSendRawCallback(py::object &callable) {
  py_onSendRaw.reset(callable);
}

void Server::onSendRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onSendRaw.is_set()) {
    // create a copy
    auto cp = std::shared_ptr<char[]>(new char[msgSize]);
    memcpy(cp.get(), msg, msgSize);

    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    executor->add(SAFE_LAMBDA_CAPTURE(
        {
          DEBUG_PRINT(Debug::Server, "CALLBACK on_send_raw");
          Module::ScopedGilAcquire const scoped("Server.on_send_raw");
          PyObject *pymemview =
              PyMemoryView_FromMemory(cp.get(), msgSize, PyBUF_READ);
          PyObject *pybytes = PyBytes_FromObject(pymemview);

          self->py_onSendRaw.call(self, py::handle(pybytes));
        },
        cp, msgSize));
  }
}

void Server::setOnClockSyncCallback(py::object &callable) {
  py_onClockSync.reset(callable);
}

CommandResponseState Server::onClockSync(const std::string _ip,
                                         const Object::DateTime time) {
  if (py_onClockSync.is_set()) {
    DEBUG_PRINT(Debug::Server, "CALLBACK on_clock_sync");
    Module::ScopedGilAcquire const scoped("Server.on_clock_sync");

    if (py_onClockSync.call(shared_from_this(), _ip, time)) {
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
    const Remote::Message::InvalidMessageException &exception) {
  const auto message = exception.getMessage();
  const auto cause = exception.getCause();
  CS101_ASDU asdu = message->getAsdu();

  // manipulate and send copy instead of original ASDU
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setNegative(cp, true);
  switch (cause) {
  case INVALID_TYPE_ID:
  case UNKNOWN_TYPE_ID:
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] Invalid type id");
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
  default: {
    DEBUG_PRINT(Debug::Server, "on_unexpected_message] " + exception.getWhat());
  }
  }
  CS101_ASDU_destroy(cp);

  if (py_onUnexpectedMessage.is_set()) {
    std::weak_ptr<Server> weakSelf =
        shared_from_this(); // Weak reference to Server
    executor->add(SAFE_LAMBDA_CAPTURE(
        {
          DEBUG_PRINT(Debug::Server, "CALLBACK on_unexpected_message");
          Module::ScopedGilAcquire const scoped("Server.on_unexpected_message");
          self->py_onUnexpectedMessage.call(self, message, cause);
        },
        message, cause));
  }
}

void Server::setOnConnectCallback(py::object &callable) {
  py_onConnect.reset(callable);
}

std::uint_fast16_t Server::getTickRate_ms() const { return tickRate_ms; }

bool Server::connectionRequestHandler(void *parameter, const char *ipAddress) {
  const auto instance = getInstance(parameter);
  if (!instance) {
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

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore connection event " +
                                   PeerConnectionEvent_toString(event) +
                                   " during shutdown");
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
    } else if (event == CS104_CON_EVENT_ACTIVATED) {
      // set as a valid receiver
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
                      const CS101_CauseOfTransmission cause,
                      const bool timestamp) {
  const auto category = point->getInfo()->getCategory();
  // lazy confirm previous control command
  if (category != MONITORING_STATUS && category != MONITORING_COUNTER &&
      category != MONITORING_EVENT) {
    // todo is this a valid option or do we auto-confirm anyway?
    throw std::invalid_argument("Only monitoring points are supported");
    // auto message = Message::OutgoingMessage::create(std::move(point));
    // message->setCauseOfTransmission(cause);
    // return send(std::move(message));
  }

  // report monitoring point
  auto message = Message::OutgoingMessage::create(std::move(point));
  message->setCauseOfTransmission(cause);
  return send(std::move(message));
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

bool Server::sendBatch(std::shared_ptr<Remote::Message::Batch> batch,
                       IMasterConnection connection) {
  if (!enabled.load() || !hasActiveConnections())
    return false;

  Module::ScopedGilRelease const scoped("Server.sendBatch");

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  if (!batch->hasPoints()) {
    DEBUG_PRINT(Debug::Server, "Empty batch");
    return false;
  }

  if (connection) {
    auto param = IMasterConnection_getApplicationLayerParameters(connection);
    batch->setOriginatorAddress(param->originatorAddress);
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
      auto message = Remote::Message::OutgoingMessage::create(point);

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
    end = std::chrono::steady_clock::now();
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
  if (!isExistingConnection(connection))
    return;

  // manipulate copy instead of original asdu
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setCOT(cp, CS101_COT_ACTIVATION_CON);
  CS101_ASDU_setNegative(cp, negative);

  if (isGlobalCommonAddress(CS101_ASDU_getCA(asdu))) {
    DEBUG_PRINT(Debug::Server, "send_activation_confirmation] to all MTUs");
    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    for (auto &s : stations) {
      CS101_ASDU_setCA(cp, s->getCommonAddress());
      IMasterConnection_sendASDU(connection, cp);
    }
  } else {
    DEBUG_PRINT(Debug::Server,
                "send_activation_confirmation] to requesting MTU");
    IMasterConnection_sendASDU(connection, cp);
  }
  CS101_ASDU_destroy(cp);
}

void Server::sendActivationTermination(IMasterConnection connection,
                                       CS101_ASDU asdu) {
  if (!isExistingConnection(connection))
    return;

  // manipulate copy instead of original asdu
  CS101_ASDU cp = CS101_ASDU_clone(asdu, nullptr);
  CS101_ASDU_setCOT(cp, CS101_COT_ACTIVATION_TERMINATION);

  if (isGlobalCommonAddress(CS101_ASDU_getCA(asdu))) {

    std::lock_guard<Module::GilAwareMutex> const st_lock(station_mutex);
    for (auto &s : stations) {
      CS101_ASDU_setCA(cp, s->getCommonAddress());
      IMasterConnection_sendASDU(connection, cp);
    }
  } else {
    IMasterConnection_sendASDU(connection, cp);
  }
  CS101_ASDU_destroy(cp);
}

void Server::sendEndOfInitialization(const std::uint_fast16_t commonAddress,
                                     const CS101_CauseOfInitialization cause) {

  Module::ScopedGilRelease const scoped("Server.send");

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  auto io = EndOfInitialization_create(nullptr, static_cast<uint8_t>(cause));

  CS101_ASDU asdu =
      CS101_ASDU_create(appLayerParameters, false, CS101_COT_INITIALIZED, 0,
                        commonAddress, false, false);
  CS101_ASDU_addInformationObject(asdu, (InformationObject)io);
  CS104_Slave_enqueueASDU(slave, asdu);
  CS101_ASDU_destroy(asdu);
  EndOfInitialization_destroy(io);

  if (debug) {
    end = std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "send] Send M_EI_NA_1 | COI: " + CauseOfInitialization_toString(cause) +
            " | TOTAL " + TICTOC(begin, end));
  }
}

void Server::sendPeriodicInventory() {
  if (!enabled.load() || !hasActiveConnections())
    return;

  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;

  // always initialize begin, as it is used for cyclic report interval
  begin = std::chrono::steady_clock::now();

  bool empty = true;
  IEC60870_5_TypeID type = C_TS_TA_1;
  std::shared_ptr<Object::Information::IInformation> info;

  // batch messages per station by type
  std::map<IEC60870_5_TypeID, std::shared_ptr<Remote::Message::Batch>> batchMap;

  for (const auto &station : getStations()) {
    for (const auto &point : station->getPoints()) {
      info = point->getInfo();
      type = Transformer::asType(info, false);

      // only monitoring status points
      if (info->getCategory() != MONITORING_STATUS)
        continue;

      // point has the cyclic report enabled and is ready for transmission?
      const auto next = point->nextReportAt();
      if (!next.has_value() || begin < next.value())
        continue;

      try {
        // add the message to a group
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
    end = std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Server,
                          "auto_transmit] TOTAL " + TICTOC(begin, end));
  }
}

std::optional<std::uint8_t> Server::getSelector(const uint16_t ca,
                                                const uint32_t ioa) const {
  const auto selection = selectionManager.get(ca, ioa);
  if (selection.has_value()) {
    return selection.value().oa;
  }
  return std::nullopt;
}

std::shared_ptr<Remote::Message::IncomingMessage>
Server::getValidMessage(CS101_ASDU asdu) {
  try {
    auto message = Remote::Message::IncomingMessage::create(
        asdu, appLayerParameters, true);

    // test NEGATIVE
    if (message->isNegative()) {
      throw Remote::Message::InvalidMessageException(
          message, CUSTOM_NOT_SUPPORTED, "Negative message received");
      ;
    }

    // test COT
    if (!message->isValidCauseOfTransmission()) {
      throw Remote::Message::InvalidMessageException(message, INVALID_COT);
    }

    // test CA
    const auto station = getStation(message->getCommonAddress());
    if (!station) {
      throw Remote::Message::InvalidMessageException(message, UNKNOWN_CA);
    }

    // inject station timezone into DateTime properties
    message->getInfo()->injectTimeZone(station->getTimeZoneOffset(),
                                       station->isDaylightSavingTime());

    return std::move(message);
  } catch (const std::exception &e) {
    // fail-safe load without parsing any information object
    auto message = Remote::Message::IncomingMessage::create(
        asdu, appLayerParameters, false);
    throw Remote::Message::InvalidMessageException(
        message, CUSTOM_NOT_SUPPORTED, std::string(e.what()));
  }
}

void Server::rawMessageHandler(void *parameter, IMasterConnection connection,
                               uint_fast8_t *msg, int msgSize, bool sent) {
  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore raw message during shutdown");
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
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore interrogation command during shutdown");
    return true;
  }

  try {
    const auto message = instance->getValidMessage(asdu);

    if (qoi < IEC60870_QOI_STATION || qoi > IEC60870_QOI_GROUP_16) {
      // invalid group, reject command
      throw Remote::Message::InvalidMessageException(
          message, CUSTOM_NOT_SUPPORTED, "Invalid qualifier of interrogation");
    }

    // confirm activation
    instance->sendActivationConfirmation(connection, asdu, false);

    const auto commonAddress = message->getCommonAddress();
    IEC60870_5_TypeID type = C_TS_TA_1;
    std::shared_ptr<Object::Information::IInformation> info;

    // batch messages per station by type
    std::map<IEC60870_5_TypeID, std::shared_ptr<Remote::Message::Batch>>
        batchMap;

    const size_t group_id = qoi - IEC60870_QOI_STATION;
    const CS101_CauseOfTransmission cot =
        static_cast<CS101_CauseOfTransmission>(qoi);

    for (const auto &station : instance->getStations()) {
      if (isGlobalCommonAddress(commonAddress) ||
          station->getCommonAddress() == commonAddress) {

        for (const auto &point : station->getGroup(group_id)) {
          info = point->getInfo();
          type = Transformer::asType(info, false);

          // only monitoring status points
          if (info->getCategory() != MONITORING_STATUS)
            continue;

          try {
            // add the message to a group
            auto g = batchMap.find(type);
            if (g == batchMap.end()) {
              auto b = Remote::Message::Batch::create(cot);
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

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
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

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server,
                "Ignore counter interrogation command during shutdown");
    return true;
  }

  try {
    const auto message = instance->getValidMessage(asdu);

    const auto rqt =
        static_cast<CS101_QualifierOfCounterInterrogation>(qcc & 0b00111111);
    const auto frz = static_cast<CS101_FreezeOfCounterInterrogation>(
        (qcc >> 6) & 0b00000011);

    if (rqt < CS101_QualifierOfCounterInterrogation::GROUP_1 ||
        rqt > CS101_QualifierOfCounterInterrogation::GENERAL) {
      // invalid group, reject command
      throw Remote::Message::InvalidMessageException(
          message, CUSTOM_NOT_SUPPORTED,
          "Invalid qualifier of counter interrogation");
    }

    bool const is_general =
        static_cast<std::uint_fast8_t>(rqt) ==
        static_cast<int>(CS101_QualifierOfCounterInterrogation::GENERAL);
    const size_t group_id = is_general ? 0 : static_cast<size_t>(rqt);
    const CS101_CauseOfTransmission cot =
        is_general ? CS101_COT_REQUESTED_BY_GENERAL_COUNTER
                   : static_cast<CS101_CauseOfTransmission>(
                         static_cast<std::uint_fast8_t>(
                             CS101_COT_REQUESTED_BY_GENERAL_COUNTER) +
                         static_cast<std::uint_fast8_t>(rqt));

    // confirm activation
    instance->sendActivationConfirmation(connection, asdu, false);

    const auto commonAddress = message->getCommonAddress();
    IEC60870_5_TypeID type = C_TS_TA_1;
    std::shared_ptr<Object::Information::IInformation> info;

    // batch messages per station by type
    std::map<IEC60870_5_TypeID, std::shared_ptr<Remote::Message::Batch>>
        batchMap;

    for (const auto &station : instance->getStations()) {
      if (isGlobalCommonAddress(commonAddress) ||
          station->getCommonAddress() == commonAddress) {

        for (const auto &point : station->getGroup(group_id)) {
          info = point->getInfo();
          type = Transformer::asType(info, false);
          if (info->getCategory() != MONITORING_COUNTER)
            continue;

          // todo Wahlmöglichkeiten ZÄHLWERT SPEICHERN und INKREMENTALWERT
          // SPEICHERN werden benutzt. Die Zählwerte werden mit der
          // ÜBERTRAGUNGSURSACHE = SPONTAN nach dem Speichern übertragen ABFRAGE
          // ZÄHLWERTE wird benutzt. In diesem Fall werden die Zählwerte mit der
          // ÜBERTRAGUNGSURSACHE = ABGEFRAGT DURCH ZÄHLERABFRAGE übertragen.
          // todo report_ms on M_IT_TA_1 SPONTANEOUS einzelmeldung
          auto current_info =
              std::dynamic_pointer_cast<Object::Information::BinaryCounterInfo>(
                  point->getInfo());
          switch (frz) {
          case CS101_FreezeOfCounterInterrogation::COUNTER_RESET:
            current_info->reset();
            continue;
          case CS101_FreezeOfCounterInterrogation::FREEZE_WITHOUT_RESET:
            current_info->freeze(false);
            continue;
          case CS101_FreezeOfCounterInterrogation::FREEZE_WITH_RESET:
            current_info->freeze(true);
            continue;
          }

          try {
            // add the message to a group
            auto g = batchMap.find(type);
            if (g == batchMap.end()) {
              auto b = Remote::Message::Batch::create(cot);
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

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
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

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore read command during shutdown");
    return true;
  }

  try {
    const auto message = instance->getValidMessage(asdu);
    const auto station = instance->getStation(message->getCommonAddress());
    if (!station) {
      throw Remote::Message::InvalidMessageException(message, UNKNOWN_CA);
    }

    auto point = station->getPoint(message->getIOA());
    if (!point) {
      throw Remote::Message::InvalidMessageException(message, UNKNOWN_IOA);
    }

    auto info = point->getInfo();
    const auto category = info->getCategory();
    // according to standard: read isn't allowed for integrated totals and
    // protection equipment events
    if (category != MONITORING_STATUS) {
      throw Remote::Message::InvalidMessageException(message, UNKNOWN_IOA);
    }

    std::weak_ptr<Server> weakSelf = instance; // Weak reference to Server
    instance->executor->add(SAFE_LAMBDA_CAPTURE(
        {
          try {
            // value polling callback
            point->onBeforeRead();

            // according to standard: send with COT=5 and without a timestamp
            self->transmit(point, CS101_COT_REQUEST, false);
          } catch (const std::exception &e) {
            std::cerr << "[c104.Server] read] Auto respond failed for "
                      << point->getInfo()->name() << " at IOA "
                      << point->getInformationObjectAddress() << ": "
                      << e.what() << std::endl;
          }
        },
        point));

    instance->sendActivationConfirmation(connection, asdu, false);

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
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

bool Server::clockSyncHandler(void *parameter, IMasterConnection connection,
                              CS101_ASDU asdu, CP56Time2a time) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore clock-sync command during shutdown");
    return true;
  }

  try {
    const auto datetime = Object::DateTime(time);

    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(debug, Debug::Server,
                          "clock_sync_handler] TIME " + datetime.toString());

    // execute python callback
    CommandResponseState responseState =
        instance->onClockSync(std::string(ipAddrStr), datetime);

    if (responseState != RESPONSE_STATE_NONE) {
      // send confirmation
      instance->sendActivationConfirmation(
          connection, asdu, (responseState == RESPONSE_STATE_FAILURE));
    }

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "clock_sync_handler] IP " + std::string(ipAddrStr) + " | OA " +
            std::to_string(CS101_ASDU_getOA(asdu)) + " | CA " +
            std::to_string(CS101_ASDU_getCA(asdu)) + " | TOTAL " +
            TICTOC(begin, end));
  }
  return true;
}

bool Server::resetProcessHandler(void *parameter, IMasterConnection connection,
                                 CS101_ASDU asdu, QualifierOfRPC qualifier) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore reset-process command during shutdown");
    return true;
  }

  try {
    const auto message = instance->getValidMessage(asdu);
    // todo not implemented
    // execute python callback
    // responseState = instance->onProcessReset(std::string(ipAddrStr), ...);
    throw Remote::Message::InvalidMessageException(
        message, CUSTOM_NOT_SUPPORTED, "Reset process command not implemented");

    // execute python callback
    CommandResponseState responseState = RESPONSE_STATE_FAILURE;
    // responseState = instance->onResetProcess(std::string(ipAddrStr),
    // datetime);

    if (responseState != RESPONSE_STATE_NONE) {
      // send confirmation
      instance->sendActivationConfirmation(
          connection, asdu, (responseState == RESPONSE_STATE_FAILURE));
    }

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "reset_process_handler] IP " + std::string(ipAddrStr) + " | OA " +
            std::to_string(CS101_ASDU_getOA(asdu)) + " | CA " +
            std::to_string(CS101_ASDU_getCA(asdu)) + " | TOTAL " +
            TICTOC(begin, end));
  }
  return true;
}

bool Server::delayAcquisitionHandler(void *parameter,
                                     IMasterConnection connection,
                                     CS101_ASDU asdu, CP16Time2a delay) {
  bool const debug = DEBUG_TEST(Debug::Server);
  std::chrono::steady_clock::time_point begin, end;
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server,
                "Ignore delay-acquisition command during shutdown");
    return true;
  }

  try {
    const auto message = instance->getValidMessage(asdu);
    // todo not implemented
    // execute python callback
    // responseState = instance->onProcessReset(std::string(ipAddrStr), ...);
    throw Remote::Message::InvalidMessageException(
        message, CUSTOM_NOT_SUPPORTED,
        "Delay acquisition command not implemented");

    // execute python callback
    CommandResponseState responseState = RESPONSE_STATE_FAILURE;
    // responseState = instance->onDelayAcquisition(std::string(ipAddrStr),
    // datetime);

    if (responseState != RESPONSE_STATE_NONE) {
      // send confirmation
      instance->sendActivationConfirmation(
          connection, asdu, (responseState == RESPONSE_STATE_FAILURE));
    }

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    char ipAddrStr[60];
    IMasterConnection_getPeerAddress(connection, ipAddrStr, 60);

    DEBUG_PRINT_CONDITION(
        true, Debug::Server,
        "delay_acquisition_handler] IP " + std::string(ipAddrStr) + " | OA " +
            std::to_string(CS101_ASDU_getOA(asdu)) + " | CA " +
            std::to_string(CS101_ASDU_getCA(asdu)) + " | TOTAL " +
            TICTOC(begin, end));
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

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Server, "Ignore ASDU during shutdown");
    return true;
  }

  try {
    CommandResponseState responseState = RESPONSE_STATE_FAILURE;

    // message with more than one object is not allowed for command type ids
    const auto message = instance->getValidMessage(asdu);
    const auto station = instance->getStation(message->getCommonAddress());
    if (!station) {
      throw Remote::Message::InvalidMessageException(message, UNKNOWN_CA);
    }

    const auto point = station->getPoint(message->getIOA());
    if (!point) {
      throw Remote::Message::InvalidMessageException(message, UNKNOWN_IOA);
    }

    const auto cmd = std::dynamic_pointer_cast<Object::Information::ICommand>(
        message->getInfo());
    if (!cmd) {
      throw Remote::Message::InvalidMessageException(
          message, CUSTOM_NOT_SUPPORTED, "Only commands supported");
    }
    if (cmd->name().compare(point->getInfo()->name()) != 0) {
      throw Remote::Message::InvalidMessageException(
          message, INVALID_TYPE_ID,
          "Mismatching type between command and point");
    }

    if (message->isSelectCommand()) {
      instance->select(message, point);
      responseState = RESPONSE_STATE_SUCCESS;
    } else {
      responseState = instance->execute(message, point);
    }

    // send confirmation
    if ((responseState != RESPONSE_STATE_NONE) &&
        message->requireConfirmation()) {
      instance->sendActivationConfirmation(
          connection, asdu, (responseState == RESPONSE_STATE_FAILURE));
    }

  } catch (const Remote::Message::InvalidMessageException &e) {
    // report error cause
    instance->onUnexpectedMessage(connection, e);
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

std::string Server::toString() const {
  size_t lencon = 0;
  {
    std::scoped_lock<Module::GilAwareMutex> const lock(connection_mutex);
    lencon = connectionMap.size();
  }
  size_t lenst = 0;
  {
    std::scoped_lock<Module::GilAwareMutex> const lock(station_mutex);
    lenst = stations.size();
  }
  std::ostringstream oss;
  oss << "<104.Server ip=" << ip << ", port=" << std::to_string(port)
      << ", #clients=" << std::to_string(lencon)
      << ", #stations=" << std::to_string(lenst) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
};
