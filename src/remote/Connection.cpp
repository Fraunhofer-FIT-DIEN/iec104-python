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
 * @file Connection.cpp
 * @brief manage 60870-5-104 connection from scada to a remote terminal unit
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Connection.h"
#include "Client.h"
#include "module/ScopedGilAcquire.h"
#include "module/ScopedGilRelease.h"
#include "remote/Helper.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/OutgoingMessage.h"
#include "remote/message/PointCommand.h"

using namespace Remote;
using namespace std::chrono_literals;

// Define static members
std::unordered_map<void *, std::weak_ptr<Connection>> Connection::instanceMap;
std::mutex Connection::instanceMapMutex;

Connection::Connection(
    std::shared_ptr<Client> _client, const std::string &_ip,
    const uint_fast16_t _port, const uint_fast16_t command_timeout_ms,
    const ConnectionInit _init,
    std::shared_ptr<Remote::TransportSecurity> transport_security,
    const uint_fast8_t originator_address)
    : client(_client), ip(_ip), port(_port), init(_init),
      commandTimeout_ms(command_timeout_ms) {
  Assert_IPv4(_ip);
  Assert_Port(_port);

  if (transport_security) {
    connection = CS104_Connection_createSecure(ip.c_str(), _port,
                                               transport_security->get());
  } else {
    connection = CS104_Connection_create(ip.c_str(), _port);
  }
  connectionString = connectionStringFormatter(ip, _port);

  // use lib60870-C defaults for connection timeouts
  // auto param = CS104_Connection_getAPCIParameters(connection);
  // param->t0 = 10; // socket connect timeout
  // param->t1 = 15; // acknowledgement timeout
  // param->t2 = 10; // acknowledgement interval

  if (originator_address > 0) {
    setOriginatorAddress(originator_address);
  }

  void *key = static_cast<void *>(this);
  CS104_Connection_setRawMessageHandler(connection,
                                        &Connection::rawMessageHandler, key);
  CS104_Connection_setConnectionHandler(connection,
                                        &Connection::connectionHandler, key);
  CS104_Connection_setASDUReceivedHandler(connection, &Connection::asduHandler,
                                          key);
  DEBUG_PRINT(Debug::Connection, "Created");
}

Connection::~Connection() {
  {
    std::scoped_lock<Module::GilAwareMutex> const lock(stations_mutex);
    stations.clear();
  }

  {
    std::lock_guard<std::mutex> lock(instanceMapMutex);
    instanceMap.erase(
        static_cast<void *>(this)); // Remove from map on destruction
  }

  CS104_Connection_destroy(connection);
  DEBUG_PRINT(Debug::Connection, "Removed");
}

std::shared_ptr<Connection> Connection::getInstance(void *key) {
  std::lock_guard<std::mutex> lock(instanceMapMutex);
  auto it = instanceMap.find(key);
  if (it != instanceMap.end()) {
    return it->second.lock();
  }
  return {nullptr};
}

std::string Connection::getConnectionString() const { return connectionString; }

std::string Connection::getIP() const { return ip; }

std::uint_fast16_t Connection::getPort() const { return port; }

ConnectionState Connection::getState() const { return state.load(); }

void Connection::setState(ConnectionState connectionState) {
  ConnectionState const prev = state.load();
  if (prev != connectionState) {
    state.store(connectionState);
    if (py_onStateChange.is_set()) {
      if (auto c = getClient()) {
        std::weak_ptr<Connection> weakSelf =
            shared_from_this(); // Weak reference to `this`
        c->scheduleTask([weakSelf, connectionState]() {
          auto self = weakSelf.lock();
          if (!self)
            return; // Prevent running if `this` was destroyed

          DEBUG_PRINT(Debug::Connection, "CALLBACK on_state_change");
          Module::ScopedGilAcquire const scoped("Connection.on_state_change");
          self->py_onStateChange.call(self, connectionState);
        });
      }
    }
    DEBUG_PRINT(Debug::Connection,
                "state] " + ConnectionState_toString(prev) + " -> " +
                    ConnectionState_toString(connectionState));
  }
}

void Connection::setOriginatorAddress(uint_fast8_t address) {
  std::uint_fast8_t const prev = originatorAddress.load();
  if (prev != address) {
    originatorAddress.store(address);

    std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);
    auto parameters = CS104_Connection_getAppLayerParameters(connection);
    parameters->originatorAddress = address;
    CS104_Connection_setAppLayerParameters(connection, parameters);
  }
}

std::uint_fast8_t Connection::getOriginatorAddress() const {
  return originatorAddress.load();
}

/// @brief client object reference
std::shared_ptr<Client> Connection::getClient() const {
  if (client.expired()) {
    return {nullptr};
  }
  return client.lock();
}

void Connection::connect() {
  ConnectionState const current = state.load();
  if (current == OPEN || current == OPEN_MUTED || CLOSED_AWAIT_OPEN == current)
    return;

  if (current == OPEN_AWAIT_CLOSED) {
    DEBUG_PRINT(Debug::Connection,
                "connect] Wait for closing before reconnecting to " +
                    getConnectionString());
    setState(CLOSED_AWAIT_OPEN);
    return;
  }

  DEBUG_PRINT(Debug::Connection,
              "connect] Asynchronous connect to " + getConnectionString());

  // connect
  setState(CLOSED_AWAIT_OPEN);

  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);

  // free connection thread if exists
  CS104_Connection_close(connection);

  // reconnect
  CS104_Connection_connectAsync(connection);
}

void Connection::disconnect() {
  ConnectionState const current = state.load();
  if (CLOSED == current || OPEN_AWAIT_CLOSED == current) {
    return;
  }

  if (current == CLOSED_AWAIT_OPEN) {
    DEBUG_PRINT(Debug::Connection,
                "connect] Wait for opening before closing to " +
                    getConnectionString());
    setState(OPEN_AWAIT_CLOSED);
    return;
  }

  if (CLOSED_AWAIT_RECONNECT == current) {
    setState(CLOSED);
  } else {
    setState(OPEN_AWAIT_CLOSED);
  }

  // free connection thread
  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  CS104_Connection_close(connection);
  lock.unlock();

  // print
  DEBUG_PRINT(Debug::Connection,
              "disconnect] Disconnect from " + getConnectionString());
}

bool Connection::isOpen() const {
  ConnectionState const current = state.load();
  return current == OPEN || current == OPEN_MUTED;
}

bool Connection::isMuted() const { return state == OPEN_MUTED; }

bool Connection::mute() {
  Module::ScopedGilRelease const scoped("Connection.mute");

  if (!isOpen())
    return false;

  // print
  DEBUG_PRINT(Debug::Connection,
              "mute] Mute connection to " + getConnectionString());

  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);
  CS104_Connection_sendStopDT(connection);
  return true;
}

bool Connection::unmute() {
  Module::ScopedGilRelease const scoped("Connection.unmute");

  if (!isOpen())
    return false;

  // print
  DEBUG_PRINT(Debug::Connection,
              "unmute] Unmute connection to " + getConnectionString());

  std::lock_guard<Module::GilAwareMutex> const lock(connection_mutex);
  CS104_Connection_sendStartDT(connection);
  return true;
}

void Connection::setMuted(bool value) {
  ConnectionState current = state.load();
  if (OPEN == current && value) {
    // print
    DEBUG_PRINT(Debug::Connection,
                "set_muted] Muted connection to " + getConnectionString());
    setState(OPEN_MUTED);
  } else if (OPEN_MUTED == current && !value) {
    // print
    DEBUG_PRINT(Debug::Connection,
                "set_muted] Unmuted connection to " + getConnectionString());

    switch (init) {
    case INIT_ALL:
      if (auto c = getClient()) {

        std::weak_ptr<Connection> weakSelf =
            shared_from_this(); // Weak reference to `this`
        c->scheduleTask(
            [weakSelf]() {
              auto self = weakSelf.lock();
              if (!self)
                return; // Prevent running if `this` was destroyed

              self->interrogation(IEC60870_GLOBAL_COMMON_ADDRESS,
                                  CS101_COT_ACTIVATION, QOI_STATION, true);
              self->clockSync(IEC60870_GLOBAL_COMMON_ADDRESS, true);
              self->setState(OPEN);
            },
            -1);
      }
      break;
    case INIT_INTERROGATION:
      if (auto c = getClient()) {
        std::weak_ptr<Connection> weakSelf =
            shared_from_this(); // Weak reference to `this`
        c->scheduleTask(
            [weakSelf]() {
              auto self = weakSelf.lock();
              if (!self)
                return; // Prevent running if `this` was destroyed

              self->interrogation(IEC60870_GLOBAL_COMMON_ADDRESS,
                                  CS101_COT_ACTIVATION, QOI_STATION, true);
              self->setState(OPEN);
            },
            -1);
      }
      break;
    case INIT_CLOCK_SYNC:
      if (auto c = getClient()) {
        std::weak_ptr<Connection> weakSelf =
            shared_from_this(); // Weak reference to `this`
        c->scheduleTask(
            [weakSelf]() {
              auto self = weakSelf.lock();
              if (!self)
                return; // Prevent running if `this` was destroyed

              self->clockSync(IEC60870_GLOBAL_COMMON_ADDRESS, true);
              self->setState(OPEN);
            },
            -1);
      }
      break;
    default:
      setState(OPEN);
    }
  }
}

void Connection::setOpen() {
  // DO NOT LOCK connection_mutex: connect locks
  ConnectionState current = state.load();

  if (OPEN == current || OPEN_MUTED == current) {
    // print
    DEBUG_PRINT(Debug::Connection,
                "set_open] Already opened to " + getConnectionString());
    return;
  }

  if (OPEN_AWAIT_CLOSED == current) {
    if (auto c = getClient()) {
      std::weak_ptr<Connection> weakSelf =
          shared_from_this(); // Weak reference to `this`
      c->scheduleTask(
          [weakSelf]() {
            auto self = weakSelf.lock();
            if (!self)
              return; // Prevent running if `this` was destroyed

            self->disconnect();
          },
          -1);
    }
    return;
  }

  if (INIT_MUTED != init) {
    if (auto c = getClient()) {
      std::weak_ptr<Connection> weakSelf =
          shared_from_this(); // Weak reference to `this`
      c->scheduleTask(
          [weakSelf]() {
            auto self = weakSelf.lock();
            if (!self)
              return; // Prevent running if `this` was destroyed

            self->unmute();
          },
          -1);
    }
  }
  connectionCount++;
  connectedAt.store(std::chrono::system_clock::now());
  setState(OPEN_MUTED);

  DEBUG_PRINT(Debug::Connection,
              "set_open] Opened connection to " + getConnectionString());
}

void Connection::setClosed() {
  // DO NOT LOCK connection_mutex: disconnect locks
  ConnectionState const current = state.load();

  if (CLOSED == current) {
    // print
    DEBUG_PRINT(Debug::Connection,
                "set_closed] Already closed to " + getConnectionString());
    return;
  }

  if (CLOSED_AWAIT_OPEN != current && CLOSED_AWAIT_RECONNECT != current) {
    // set disconnected if connected previously
    disconnectedAt.store(std::chrono::system_clock::now());
  }

  // controlled close or connection lost?
  if (OPEN_AWAIT_CLOSED == current) {
    setState(CLOSED);
  } else {
    setState(CLOSED_AWAIT_RECONNECT);
    if (auto c = getClient()) {

      std::weak_ptr<Connection> weakSelf =
          shared_from_this(); // Weak reference to `this`
      c->scheduleTask(
          [weakSelf]() {
            auto self = weakSelf.lock();
            if (!self)
              return; // Prevent running if `this` was destroyed

            self->connect();
          },
          1000);
    }
  }

  DEBUG_PRINT(Debug::Connection,
              "set_closed] Connection closed to " + getConnectionString());
}

void Connection::prepareCommandSuccess(
    const std::string &cmdId,
    CommandProcessState const process_state = COMMAND_AWAIT_CON) {
  std::lock_guard<Module::GilAwareMutex> const map_lock(
      expectedResponseMap_mutex);
  auto const it = expectedResponseMap.find(cmdId);
  if (it != expectedResponseMap.end()) {
    throw std::runtime_error("[c104.Connection] command " + cmdId +
                             " already running!");
  }
  expectedResponseMap[cmdId] = process_state;
  expectedResponseMapCount[cmdId] = 0;
}

bool Connection::awaitCommandSuccess(const std::string &cmdId) {
  bool success = false;

  std::unique_lock<Module::GilAwareMutex> map_lock(expectedResponseMap_mutex);
  auto const it = expectedResponseMap.find(cmdId);
  if (it != expectedResponseMap.end()) {

    auto const end = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(commandTimeout_ms.load());

    DEBUG_PRINT(Debug::Connection, "await_command_success] Await " + cmdId);

    while (true) {
      if (response_wait.wait_until(map_lock, end) == std::cv_status::timeout) {
        DEBUG_PRINT(Debug::Connection,
                    "await_command_success] Timeout " + cmdId);
        break; // timeout
      }

      auto const _it = expectedResponseMap.find(cmdId);

      // is result still requested?
      if (_it == expectedResponseMap.end()) {
        success = false;
        DEBUG_PRINT(Debug::Connection, "await_command_success] missing cmdId" +
                                           cmdId + ": " +
                                           std::to_string(success));
        break; // failed -> result is not needed anymore
      }

      // has final result?
      if (_it->second < COMMAND_AWAIT_CON) {
        success = _it->second == COMMAND_SUCCESS;
        DEBUG_PRINT(Debug::Connection, "await_command_success] Result " +
                                           cmdId + ": " +
                                           std::to_string(success));
        break; // result
      }

      DEBUG_PRINT(Debug::Connection,
                  "await_command_success] Non-topic " + cmdId);
    }

    // delete cmd if still valid
    auto const _it = expectedResponseMap.find(cmdId);
    if (_it != expectedResponseMap.end()) {
      expectedResponseMap.erase(_it);
    }
    // delete cmd from count if still valid
    auto const _countIt = expectedResponseMapCount.find(cmdId);
    if (_countIt != expectedResponseMapCount.end()) {
      expectedResponseMapCount.erase(_countIt);
    }

    // print
    DEBUG_PRINT(Debug::Connection, "await_command_success] Stats " + cmdId +
                                       " | TOTAL " + TICTOCNOW(end));
  }

  map_lock.unlock();
  return success;
}

void Connection::setCommandSuccess(
    std::shared_ptr<Message::IncomingMessage> message) {
  // fix for READ command
  TypeID const type = message->getCauseOfTransmission() == CS101_COT_REQUEST
                          ? C_RD_NA_1
                          : message->getType();
  ConnectionState const current = state.load();
  bool found = false;

  std::string cmdId = std::to_string(message->getCommonAddress()) + "-" +
                      TypeID_toString(type) + "-" +
                      std::to_string(message->getIOA());
  std::string const cmdIdAlt = std::to_string(IEC60870_GLOBAL_COMMON_ADDRESS) +
                               "-" + TypeID_toString(type) + "-" +
                               std::to_string(message->getIOA());

  {
    std::lock_guard<Module::GilAwareMutex> const map_lock(
        expectedResponseMap_mutex);
    auto it = expectedResponseMap.find(cmdId);
    if (it == expectedResponseMap.end()) {
      // try global common address
      it = expectedResponseMap.find(cmdIdAlt);
      if (it != expectedResponseMap.end()) {
        cmdId = cmdIdAlt;
      }
    }
    if (it != expectedResponseMap.end()) {
      found = true;
      if (message->isNegative()) {
        expectedResponseMap[cmdId] = COMMAND_FAILURE;
      } else {
        switch (expectedResponseMap[cmdId]) {
        case COMMAND_AWAIT_CON:
          expectedResponseMap[cmdId] =
              (message->getCauseOfTransmission() == CS101_COT_ACTIVATION_CON)
                  ? COMMAND_SUCCESS
                  : COMMAND_FAILURE;
          break;
        case COMMAND_AWAIT_CON_TERM:
          // this case allows counting up and down for multiple responses,
          // number of CONs must match number of TERMs
          switch (message->getCauseOfTransmission()) {
          case CS101_COT_ACTIVATION_CON:
            // increase counter on new CON
            expectedResponseMapCount[cmdId]++;
            break;
          case CS101_COT_ACTIVATION_TERMINATION:
            // error, if TERM before CON
            if (expectedResponseMapCount[cmdId] < 1) {
              expectedResponseMap[cmdId] = COMMAND_FAILURE;
              break;
            }
            // decrease counter on new TERM
            expectedResponseMapCount[cmdId]--;

            // all CONs have a TERM => Success
            if (expectedResponseMapCount[cmdId] == 0) {
              expectedResponseMap[cmdId] = COMMAND_SUCCESS;
            }
            break;
          default:
            expectedResponseMap[cmdId] = COMMAND_FAILURE;
          }
          break;
        case COMMAND_AWAIT_TERM:
          expectedResponseMap[cmdId] = (message->getCauseOfTransmission() ==
                                        CS101_COT_ACTIVATION_TERMINATION)
                                           ? COMMAND_SUCCESS
                                           : COMMAND_FAILURE;
          break;
        case COMMAND_AWAIT_REQUEST:
          expectedResponseMap[cmdId] =
              (message->getCauseOfTransmission() == CS101_COT_ACTIVATION_CON ||
               message->getCauseOfTransmission() == CS101_COT_REQUEST)
                  ? COMMAND_SUCCESS
                  : COMMAND_FAILURE;
          break;
        default:
          expectedResponseMap[cmdId] = COMMAND_SUCCESS;
        }
      }
    }
  }

  // print
  DEBUG_PRINT(Debug::Connection, "set_command_success] Result " + cmdId + ": " +
                                     std::to_string(!message->isNegative()) +
                                     " | found: " + std::to_string(found));
  // notify about update
  if (found) {
    response_wait.notify_all();
  }
}

void Connection::cancelCommandSuccess(const std::string &cmdId) {
  std::lock_guard<Module::GilAwareMutex> const map_lock(
      expectedResponseMap_mutex);
  auto it = expectedResponseMap.find(cmdId);
  if (it != expectedResponseMap.end()) {
    expectedResponseMap.erase(it);
  }
}

bool Connection::hasStations() const {
  std::lock_guard<Module::GilAwareMutex> const lock(stations_mutex);

  return !stations.empty();
}

Object::StationVector Connection::getStations() const {
  std::lock_guard<Module::GilAwareMutex> const lock(stations_mutex);

  return stations;
}

std::shared_ptr<Object::Station>
Connection::getStation(const std::uint_fast16_t commonAddress) const {
  if (isGlobalCommonAddress(commonAddress)) {
    return {nullptr};
  }

  std::lock_guard<Module::GilAwareMutex> const lock(stations_mutex);
  for (auto &s : stations) {
    if (s->getCommonAddress() == commonAddress) {
      return s;
    }
  }

  return {nullptr};
}

bool Connection::hasStation(const std::uint_fast16_t commonAddress) const {
  return nullptr != getStation(commonAddress).get();
}

std::shared_ptr<Object::Station>
Connection::addStation(std::uint_fast16_t commonAddress) {
  if (hasStation(commonAddress)) {
    return {nullptr};
  }

  DEBUG_PRINT(Debug::Connection,
              "add_station] CA " + std::to_string(commonAddress));

  std::lock_guard<Module::GilAwareMutex> const lock(stations_mutex);
  auto station =
      Object::Station::create(commonAddress, nullptr, shared_from_this());

  stations.push_back(station);
  return station;
}

bool Connection::removeStation(std::uint_fast16_t commonAddress) {
  std::lock_guard<Module::GilAwareMutex> const lock(stations_mutex);

  DEBUG_PRINT(Debug::Connection,
              "remove_station] CA " + std::to_string(commonAddress));

  size_t originalSize = stations.size();

  // Use std::remove_if to find and remove the entry
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

void Connection::setOnReceiveRawCallback(py::object &callable) {
  py_onReceiveRaw.reset(callable);
}

CS104_APCIParameters Connection::getParameters() const {
  return CS104_Connection_getAPCIParameters(connection);
}

void Connection::onReceiveRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onReceiveRaw.is_set()) {
    if (auto c = getClient()) {
      // create a copy
      auto cp = std::shared_ptr<char[]>(new char[msgSize]);
      memcpy(cp.get(), msg, msgSize);

      std::weak_ptr<Connection> weakSelf =
          shared_from_this(); // Weak reference to `this`
      c->scheduleTask([weakSelf, cp, msgSize]() {
        auto self = weakSelf.lock();
        if (!self)
          return; // Prevent running if `this` was destroyed

        DEBUG_PRINT(Debug::Connection, "CALLBACK on_receive_raw");
        Module::ScopedGilAcquire const scoped("Connection.on_receive_raw");
        PyObject *pymemview =
            PyMemoryView_FromMemory(cp.get(), msgSize, PyBUF_READ);
        PyObject *pybytes = PyBytes_FromObject(pymemview);

        self->py_onReceiveRaw.call(self, py::handle(pybytes));
      });
    }
  }
}

void Connection::setOnSendRawCallback(py::object &callable) {
  py_onSendRaw.reset(callable);
}

void Connection::onSendRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onSendRaw.is_set()) {
    if (auto c = getClient()) {
      // create a copy
      auto cp = std::shared_ptr<char[]>(new char[msgSize]);
      memcpy(cp.get(), msg, msgSize);

      std::weak_ptr<Connection> weakSelf =
          shared_from_this(); // Weak reference to `this`
      c->scheduleTask([weakSelf, cp, msgSize]() {
        auto self = weakSelf.lock();
        if (!self)
          return; // Prevent running if `this` was destroyed

        DEBUG_PRINT(Debug::Connection, "CALLBACK on_send_raw");
        Module::ScopedGilAcquire const scoped("Connection.on_send_raw");
        PyObject *pymemview =
            PyMemoryView_FromMemory(cp.get(), msgSize, PyBUF_READ);
        PyObject *pybytes = PyBytes_FromObject(pymemview);

        self->py_onSendRaw.call(self, py::handle(pybytes));
      });
    }
  }
}

void Connection::setOnUnexpectedMessageCallback(py::object &callable) {
  py_onUnexpectedMessage.reset(callable);
}

void Connection::onUnexpectedMessage(
    std::shared_ptr<Message::IncomingMessage> message,
    UnexpectedMessageCause cause) {
  if (py_onUnexpectedMessage.is_set()) {
    if (auto c = getClient()) {

      std::weak_ptr<Connection> weakSelf =
          shared_from_this(); // Weak reference to `this`
      c->scheduleTask([weakSelf, message, cause]() {
        auto self = weakSelf.lock();
        if (!self)
          return; // Prevent running if `this` was destroyed

        DEBUG_PRINT(Debug::Connection, "CALLBACK on_unexpected_message");
        Module::ScopedGilAcquire const scoped(
            "Connection.on_unexpected_message");
        self->py_onUnexpectedMessage.call(self, message, cause);
      });
    }
  }
}

void Connection::setOnStateChangeCallback(py::object &callable) {
  py_onStateChange.reset(callable);
}

std::optional<std::chrono::system_clock::time_point>
Connection::getConnectedAt() const {
  if (isOpen()) {
    return connectedAt.load();
  }
  return std::nullopt;
}

std::optional<std::chrono::system_clock::time_point>
Connection::getDisconnectedAt() const {
  if (!isOpen()) {
    return disconnectedAt.load();
  }
  return std::nullopt;
}

bool Connection::interrogation(std::uint_fast16_t commonAddress,
                               CS101_CauseOfTransmission cause,
                               CS101_QualifierOfInterrogation qualifier,
                               const bool wait_for_response) {
  Module::ScopedGilRelease const scoped("Connection.interrogation");

  if (!isOpen())
    return false;

  if (qualifier < IEC60870_QOI_STATION || qualifier > IEC60870_QOI_GROUP_16)
    throw std::invalid_argument("Invalid qualifier " +
                                std::to_string(qualifier));

  std::string const cmdId = std::to_string(commonAddress) + "-C_IC_NA_1-0";
  if (wait_for_response) {
    prepareCommandSuccess(cmdId, COMMAND_AWAIT_CON_TERM);
  }

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result = CS104_Connection_sendInterrogationCommand(
      connection, cause, commonAddress, qualifier);
  lock.unlock();

  if (wait_for_response) {
    if (result) {
      return awaitCommandSuccess(cmdId);
    }
    // result not required anymore, because no message was sent
    cancelCommandSuccess(cmdId);
  }
  return result;
}

bool Connection::counterInterrogation(
    const std::uint_fast16_t commonAddress,
    const CS101_CauseOfTransmission cause,
    const CS101_QualifierOfCounterInterrogation qualifier,
    const CS101_FreezeOfCounterInterrogation freeze,
    const bool wait_for_response) {
  Module::ScopedGilRelease const scoped("Connection.counterInterrogation");

  if (!isOpen())
    return false;

  std::string const cmdId = std::to_string(commonAddress) + "-C_CI_NA_1-0";
  if (wait_for_response) {
    prepareCommandSuccess(cmdId, COMMAND_AWAIT_CON_TERM);
  }

  const auto qcc = static_cast<std::uint_fast8_t>(qualifier) |
                   (static_cast<std::uint_fast8_t>(freeze) << 6);

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result = CS104_Connection_sendCounterInterrogationCommand(
      connection, cause, commonAddress, qcc);
  lock.unlock();

  if (wait_for_response) {
    if (result) {
      return awaitCommandSuccess(cmdId);
    }
    // result not required anymore, because no message was sent
    cancelCommandSuccess(cmdId);
  }
  return result;
}

bool Connection::clockSync(std::uint_fast16_t commonAddress,
                           const bool wait_for_response) {
  Module::ScopedGilRelease const scoped("Connection.clockSync");

  if (!isOpen())
    return false;

  std::string const cmdId = std::to_string(commonAddress) + "-C_CS_NA_1-0";
  if (wait_for_response) {
    prepareCommandSuccess(cmdId);
  }

  sCP56Time2a time{};
  from_time_point(&time, std::chrono::system_clock::now());

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result =
      CS104_Connection_sendClockSyncCommand(connection, commonAddress, &time);
  lock.unlock();

  if (wait_for_response) {
    if (result) {
      return awaitCommandSuccess(cmdId);
    }
    // result not required anymore, because no message was sent
    cancelCommandSuccess(cmdId);
  }
  return result;
}

bool Connection::test(std::uint_fast16_t commonAddress, bool with_time,
                      const bool wait_for_response) {
  Module::ScopedGilRelease const scoped("Connection.test");

  if (!isOpen())
    return false;

  std::string const cmdId = std::to_string(commonAddress) + "-C_TS_TA_1-0";
  if (wait_for_response) {
    prepareCommandSuccess(cmdId);
  }

  if (with_time) {
    sCP56Time2a time{};
    from_time_point(&time, std::chrono::system_clock::now());

    std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
    bool const result = CS104_Connection_sendTestCommandWithTimestamp(
        connection, commonAddress, testSequenceCounter++, &time);
    lock.unlock();

    if (wait_for_response) {
      if (result) {
        return awaitCommandSuccess(cmdId);
      }
      // result not required anymore, because no message was sent
      cancelCommandSuccess(cmdId);
    }
    return result;
  }

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result =
      CS104_Connection_sendTestCommand(connection, commonAddress);
  lock.unlock();

  if (wait_for_response) {
    if (result) {
      return awaitCommandSuccess(cmdId);
    }
    // result not required anymore, because no message was sent
    cancelCommandSuccess(cmdId);
  }
  return result;
}

bool Connection::transmit(std::shared_ptr<Object::DataPoint> point,
                          const CS101_CauseOfTransmission cause) {
  auto type = point->getType();

  // is a supported control command?
  if (type <= S_IT_TC_1 || type >= M_EI_NA_1) {
    throw std::invalid_argument("Invalid point type");
  }

  bool selectAndExecute = point->getCommandMode() == SELECT_AND_EXECUTE_COMMAND;
  // send select command
  if (selectAndExecute) {
    auto message = Message::PointCommand::create(point, true);
    message->setCauseOfTransmission(cause);
    // Select success ?
    if (!command(std::move(message), true)) {
      return false;
    }
  }
  // send execute command

  auto message = Message::PointCommand::create(point, false);
  message->setCauseOfTransmission(cause);
  if (selectAndExecute) {
    // wait for ACT_TERM after ACT_CON
    return command(std::move(message), true, COMMAND_AWAIT_CON_TERM);
  }
  return command(std::move(message), true);
}

bool Connection::command(std::shared_ptr<Message::OutgoingMessage> message,
                         const bool wait_for_response,
                         const CommandProcessState state) {
  Module::ScopedGilRelease const scoped("Connection.command");

  if (!isOpen())
    return false;

  std::string const cmdId = std::to_string(message->getCommonAddress()) + "-" +
                            TypeID_toString(message->getType()) + "-" +
                            std::to_string(message->getIOA());
  if (wait_for_response) {
    prepareCommandSuccess(cmdId, state);
  }

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result = CS104_Connection_sendProcessCommandEx(
      connection, message->getCauseOfTransmission(),
      message->getCommonAddress(), message->getInformationObject());
  lock.unlock();

  DEBUG_PRINT(Debug::Connection, "command SEND " + std::to_string(result));
  if (wait_for_response) {
    if (result) {
      return awaitCommandSuccess(cmdId);
    }
    // result not required anymore, because no message was sent
    cancelCommandSuccess(cmdId);
  }
  return result;
}

bool Connection::read(std::shared_ptr<Object::DataPoint> point,
                      const bool wait_for_response) {
  Module::ScopedGilRelease const scoped("Connection.read");

  if (!isOpen())
    return false;

  auto _station = point->getStation();
  if (!_station) {
    std::cerr << "[c104.Connection.read] Cannot get station from point"
              << std::endl;
    return false;
  }
  auto ca = _station->getCommonAddress();
  auto ioa = point->getInformationObjectAddress();

  std::string const cmdId =
      std::to_string(ca) + "-C_RD_NA_1-" + std::to_string(ioa);
  if (wait_for_response) {
    prepareCommandSuccess(cmdId, COMMAND_AWAIT_REQUEST);
  }

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result = CS104_Connection_sendReadCommand(connection, ca, ioa);
  lock.unlock();

  if (wait_for_response) {
    if (result) {
      return awaitCommandSuccess(cmdId);
    }
    // result not required anymore, because no message was sent
    cancelCommandSuccess(cmdId);
  }
  return result;
}

/* Callback handler to log sent or received messages (optional) */
void Connection::rawMessageHandler(void *parameter, uint_fast8_t *msg,
                                   const int msgSize, const bool sent) {
  std::chrono::steady_clock::time_point begin, end;
  bool const debug = DEBUG_TEST(Debug::Connection);
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Connection, "Ignore raw message in shutdown");
    return;
  }

  if (sent) {
    instance->onSendRaw(msg, msgSize);
  } else {
    instance->onReceiveRaw(msg, msgSize);
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Connection,
                          "raw_message_handler] Stats | TOTAL " +
                              TICTOC(begin, end));
  }
}

void Connection::connectionHandler(void *parameter, CS104_Connection connection,
                                   CS104_ConnectionEvent event) {
  // NEEDS TO BE THREAD SAFE!
  // For CS104 the address parameter has to be ignored
  // NEEDS TO BE ABSOLUTELY NON_BLOCKING
  std::chrono::steady_clock::time_point begin, end;
  bool const debug = DEBUG_TEST(Debug::Connection);
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT(Debug::Connection, "Ignore connection event " +
                                       ConnectionEvent_toString(event) +
                                       " in shutdown");
    return;
  }

  switch (event) {
  case CS104_CONNECTION_OPENED: {
    instance->setOpen();
    break;
  }
  case CS104_CONNECTION_FAILED:
  case CS104_CONNECTION_CLOSED: {
    instance->setClosed();
    break;
  }
  case CS104_CONNECTION_STARTDT_CON_RECEIVED: {
    instance->setMuted(false);
    break;
  }
  case CS104_CONNECTION_STOPDT_CON_RECEIVED: {
    instance->setMuted(true);
    break;
  }
  }

  if (debug) {
    end = std::chrono::steady_clock::now();
    DEBUG_PRINT_CONDITION(true, Debug::Connection,
                          "connection_handler] Connection " +
                              ConnectionEvent_toString(event) + " to " +
                              instance->getConnectionString() + " | TOTAL " +
                              TICTOC(begin, end));
  }
}

bool Connection::asduHandler(void *parameter, int address, CS101_ASDU asdu) {
  std::chrono::steady_clock::time_point begin, end;
  bool const debug = DEBUG_TEST(Debug::Connection);
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }
  bool handled = false;

  const auto instance = getInstance(parameter);
  if (!instance) {
    DEBUG_PRINT_CONDITION(debug, Debug::Connection,
                          "asdu_handler] Drop message: Connection removed");
    return false;
  }

  try {
    const auto client = instance->getClient();
    if (!client || !client->isRunning()) {
      throw std::runtime_error("Client not running");
    }

    if (!instance->isOpen()) {
      throw std::runtime_error("Connection not OPEN to " +
                               instance->getConnectionString());
    }

    const auto parameters =
        CS104_Connection_getAppLayerParameters(instance->connection);

    const auto message =
        Remote::Message::IncomingMessage::create(asdu, parameters);

    if (!message->isValidCauseOfTransmission()) {
      instance->onUnexpectedMessage(message, INVALID_COT);
      // accept invalid COT for compatibility reason
      // todo evaluate future behavior
      // throw std::domain_error("Invalid cause of transmission");
    }

    IEC60870_5_TypeID const type = message->getType();
    CS101_CauseOfTransmission const cot = message->getCauseOfTransmission();
    std::uint_fast16_t commonAddress = message->getCommonAddress();

    std::shared_ptr<Object::Station> station{};

    if (commonAddress != IEC60870_GLOBAL_COMMON_ADDRESS) {
      station = instance->getStation(commonAddress);
      if (!station) {
        // accept station via callback?
        client->onNewStation(instance, commonAddress);
        station = instance->getStation(commonAddress);
      }
    }

    // monitoring message
    if (type < C_SC_NA_1) {
      if (!station) {
        instance->onUnexpectedMessage(message, UNKNOWN_CA);
        throw std::domain_error("Unknown station");
      }

      // read command success
      if (cot == CS101_COT_REQUEST) {
        instance->setCommandSuccess(message);
      }

      while (message->next()) {
        auto point = station->getPoint(message->getIOA());
        if (!point) {
          // accept point via callback?
          client->onNewPoint(station, message->getIOA(), type);
          point = station->getPoint(message->getIOA());
        }
        if (!point) {
          // can only be reached if point was not added in on_new_point callback
          instance->onUnexpectedMessage(message, UNKNOWN_IOA);
          throw std::domain_error("Unknown point");
        }
        if (point->getType() != type) {
          instance->onUnexpectedMessage(message, MISMATCHED_TYPE_ID);
          throw std::domain_error("Mismatched TypeID");
        }
        point->onReceive(message);
        handled = true;
      }
    }

    // End of initialization
    if (M_EI_NA_1 == type) {
      if (!station) {
        instance->onUnexpectedMessage(message, UNKNOWN_CA);
        throw std::domain_error("Unknown station");
      }

      const auto io = reinterpret_cast<EndOfInitialization>(
          message->getInformationObject());
      client->onEndOfInitialization(station,
                                    static_cast<CS101_CauseOfInitialization>(
                                        EndOfInitialization_getCOI(io)));
      handled = true;
    }

    // command response
    if (type < P_ME_NA_1) {
      instance->setCommandSuccess(message);
      handled = true;
    }

    if (!handled) {
      instance->onUnexpectedMessage(message, INVALID_TYPE_ID);
    }

    if (debug) {
      end = std::chrono::steady_clock::now();
      DEBUG_PRINT_CONDITION(
          true, Debug::Connection,
          "asduHandler] Report Stats | Handled " + bool_toString(handled) +
              " | Type " + std::string(TypeID_toString(type)) + " | CA " +
              std::to_string(commonAddress) + " | TOTAL " + TICTOC(begin, end));
    }

  } catch (const std::exception &e) {
    DEBUG_PRINT_CONDITION(debug, Debug::Connection,
                          "asdu_handler] Drop message: " +
                              std::string(e.what()));
  }

  return handled;
}
