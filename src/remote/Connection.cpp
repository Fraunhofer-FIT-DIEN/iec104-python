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

  // Workaround against 10s Connect Timeout from lib60870-C down to 1s
  // @todo add python config options for APCI parameters
  auto param = CS104_Connection_getAPCIParameters(connection);
  param->t0 = 2; // Timeout for connection establishment (in s)
  // Timeout for transmitted APDUs in I/U format (in s) when
  // timeout elapsed without confirmation the connection will
  // be closed. This is used by the sender to determine if
  // the receiver has failed to confirm a message.
  param->t1 = 1;
  // Timeout to confirm messages (in s). This timeout is used by
  // the receiver to determine the time when the message
  // confirmation has to be sent.
  param->t2 = 2;
  param->t3 = 20; // time until test telegrams will be sent in case of an idle
  // connection
  CS104_Connection_setAPCIParameters(connection, param);

  if (originator_address > 0) {
    setOriginatorAddress(originator_address);
  }

  CS104_Connection_setRawMessageHandler(connection,
                                        &Connection::rawMessageHandler, this);
  CS104_Connection_setConnectionHandler(connection,
                                        &Connection::connectionHandler, this);
  CS104_Connection_setASDUReceivedHandler(connection, &Connection::asduHandler,
                                          this);
  DEBUG_PRINT(Debug::Connection, "Created");
}

Connection::~Connection() {
  {
    std::scoped_lock<Module::GilAwareMutex> const lock(stations_mutex);
    stations.clear();
  }
  CS104_Connection_destroy(connection);
  DEBUG_PRINT(Debug::Connection, "Removed");
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
        c->scheduleTask([this, connectionState]() {
          DEBUG_PRINT(Debug::Connection, "CALLBACK on_state_change");
          Module::ScopedGilAcquire const scoped("Connection.on_state_change");
          this->py_onStateChange.call(shared_from_this(), connectionState);
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
        c->scheduleTask(
            [this]() {
              this->interrogation(IEC60870_GLOBAL_COMMON_ADDRESS,
                                  CS101_COT_ACTIVATION, QOI_STATION, true);
              this->clockSync(IEC60870_GLOBAL_COMMON_ADDRESS, true);
              setState(OPEN);
            },
            -1);
      }
      break;
    case INIT_INTERROGATION:
      if (auto c = getClient()) {
        c->scheduleTask(
            [this]() {
              this->interrogation(IEC60870_GLOBAL_COMMON_ADDRESS,
                                  CS101_COT_ACTIVATION, QOI_STATION, true);
              setState(OPEN);
            },
            -1);
      }
      break;
    case INIT_CLOCK_SYNC:
      if (auto c = getClient()) {
        c->scheduleTask(
            [this]() {
              this->clockSync(IEC60870_GLOBAL_COMMON_ADDRESS, true);
              setState(OPEN);
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
      c->scheduleTask([this]() { this->disconnect(); }, -1);
    }
    return;
  }

  if (INIT_MUTED != init) {
    if (auto c = getClient()) {
      c->scheduleTask([this]() { this->unmute(); }, -1);
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
      c->scheduleTask([this]() { this->connect(); }, 1000);
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
      } else {
        auto const _it = expectedResponseMap.find(cmdId);
        // is result still requested?
        if (_it == expectedResponseMap.end()) {
          success = false;
          DEBUG_PRINT(Debug::Connection,
                      "await_command_success] missing cmdId" + cmdId + ": " +
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
    }

    // delete cmd if still valid
    auto const _it = expectedResponseMap.find(cmdId);
    if (it != expectedResponseMap.end()) {
      expectedResponseMap.erase(it);
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
          expectedResponseMap[cmdId] =
              (message->getCauseOfTransmission() == CS101_COT_ACTIVATION_CON)
                  ? COMMAND_AWAIT_TERM
                  : COMMAND_FAILURE;
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

void Connection::setOnReceiveRawCallback(py::object &callable) {
  py_onReceiveRaw.reset(callable);
}

void Connection::onReceiveRaw(unsigned char *msg, unsigned char msgSize) {
  if (py_onReceiveRaw.is_set()) {
    if (auto c = getClient()) {
      // create a copy
      auto *cp = new char[msgSize];
      memcpy(cp, msg, msgSize);

      c->scheduleTask([this, cp, msgSize]() {
        DEBUG_PRINT(Debug::Connection, "CALLBACK on_receive_raw");
        Module::ScopedGilAcquire const scoped("Connection.on_receive_raw");
        PyObject *pymemview = PyMemoryView_FromMemory(cp, msgSize, PyBUF_READ);
        PyObject *pybytes = PyBytes_FromObject(pymemview);

        this->py_onReceiveRaw.call(shared_from_this(), py::handle(pybytes));

        delete[] cp;
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
      auto *cp = new char[msgSize];
      memcpy(cp, msg, msgSize);

      c->scheduleTask([this, cp, msgSize]() {
        DEBUG_PRINT(Debug::Connection, "CALLBACK on_send_raw");
        Module::ScopedGilAcquire const scoped("Connection.on_send_raw");
        PyObject *pymemview = PyMemoryView_FromMemory(cp, msgSize, PyBUF_READ);
        PyObject *pybytes = PyBytes_FromObject(pymemview);

        this->py_onSendRaw.call(shared_from_this(), py::handle(pybytes));

        delete[] cp;
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

bool Connection::counterInterrogation(std::uint_fast16_t commonAddress,
                                      CS101_CauseOfTransmission cause,
                                      QualifierOfCIC qualifier,
                                      const bool wait_for_response) {
  Module::ScopedGilRelease const scoped("Connection.counterInterrogation");

  if (!isOpen())
    return false;

  if (qualifier < IEC60870_QOI_STATION || qualifier > IEC60870_QOI_GROUP_16)
    throw std::invalid_argument("Invalid qualifier " +
                                std::to_string(qualifier));

  std::string const cmdId = std::to_string(commonAddress) + "-C_CI_NA_1-0";
  if (wait_for_response) {
    prepareCommandSuccess(cmdId, COMMAND_AWAIT_CON_TERM);
  }

  std::unique_lock<Module::GilAwareMutex> lock(connection_mutex);
  bool const result = CS104_Connection_sendCounterInterrogationCommand(
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

  std::shared_ptr<Connection> instance{};

  try {
    instance = static_cast<Connection *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
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

  std::shared_ptr<Connection> instance{};

  try {
    instance = static_cast<Connection *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
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
  // NEEDS TO BE THREAD SAFE!
  // DON'T DESTROY ASDUs! THEY WILL BE DESTROYED AUTOMATICALLY

  std::chrono::steady_clock::time_point begin, end;
  bool const debug = DEBUG_TEST(Debug::Connection);
  if (debug) {
    begin = std::chrono::steady_clock::now();
  }

  std::shared_ptr<Connection> instance{};
  try {
    instance = static_cast<Connection *>(parameter)->shared_from_this();
  } catch (const std::bad_weak_ptr &e) {
    DEBUG_PRINT(Debug::Connection, "asdu_handler] Connection removed");
    return false;
  }

  auto client = instance->getClient();
  if (!client || !client->isRunning()) {
    DEBUG_PRINT_CONDITION(debug, Debug::Connection,
                          "asdu_handler] Client stopped");
    return false;
  }

  if (!instance->isOpen()) {
    // @todo handle invalid message??
    DEBUG_PRINT_CONDITION(debug, Debug::Connection,
                          "asdu_handler] Not connected to " +
                              instance->getConnectionString());
    return false;
  }

  auto parameters =
      CS104_Connection_getAppLayerParameters(instance->connection);

  try {
    auto message = Remote::Message::IncomingMessage::create(asdu, parameters);

    IEC60870_5_TypeID const type = message->getType();
    CS101_CauseOfTransmission const cot = message->getCauseOfTransmission();

    // monitoring message
    if (type < C_SC_NA_1) {

      // read command success
      if (cot == CS101_COT_REQUEST) {
        instance->setCommandSuccess(message);
      }

      while (message->next()) {
        auto station = instance->getStation(message->getCommonAddress());
        if (!station) {
          client->onNewStation(instance, message->getCommonAddress());
          station = instance->getStation(message->getCommonAddress());
        }
        if (station) {
          auto point = station->getPoint(message->getIOA());
          if (!point) {
            // accept point via callback?
            client->onNewPoint(station, message->getIOA(), message->getType());
            point = station->getPoint(message->getIOA());
          }
          if (point) {
            point->onReceive(message);
          } else {
            DEBUG_PRINT_CONDITION(debug, Debug::Connection,
                                  "asdu_handler] Message ignored: Unknown IOA");
          }
        } else {
          // @todo add error callback?
          DEBUG_PRINT_CONDITION(debug, Debug::Connection,
                                "asdu_handler] Message ignored: Unknown CA");
        }
      }

      if (debug) {
        end = std::chrono::steady_clock::now();
        DEBUG_PRINT_CONDITION(
            true, Debug::Connection,
            "asdu_handler] " +
                std::string(TypeID_toString(message->getType())) +
                " Report Stats"
                " | CA " +
                std::to_string(message->getCommonAddress()) + " | IOA " +
                std::to_string(message->getIOA()) + " | TOTAL " +
                TICTOC(begin, end));
      }
      return true;
    }

    // @todo handle end of initialization ? Server sends M_EI_NA_1 optionally
    if (M_EI_NA_1 == type)
      return true;

    // command response
    if (type < P_ME_NA_1) {
      instance->setCommandSuccess(message);

      if (debug) {
        end = std::chrono::steady_clock::now();
        DEBUG_PRINT_CONDITION(
            true, Debug::Connection,
            "asdu_handler] " +
                std::string(TypeID_toString(message->getType())) +
                " Response Stats" + " | CA " +
                std::to_string(message->getCommonAddress()) + " | IOA " +
                std::to_string(message->getIOA()) + " | TOTAL " +
                TICTOC(begin, end));
      }
      return true;
    }

    if (debug) {
      // @todo add python callback for unhandled messages
      end = std::chrono::steady_clock::now();
      DEBUG_PRINT_CONDITION(
          true, Debug::Connection,
          "asduHandler] Unhandled " +
              std::string(TypeID_toString(message->getType())) + " Stats" +
              " | CA " + std::to_string(message->getCommonAddress()) +
              " | IOA " + std::to_string(message->getIOA()) + " | TOTAL " +
              TICTOC(begin, end));
    }

  } catch (const std::exception &e) {
    DEBUG_PRINT(Debug::Connection, "asdu_handler] Invalid message format: " +
                                       std::string(e.what()));
  }

  return true;
}
