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
 * @file OutgoingMessage.cpp
 * @brief abstract base class for outgoing ASDU messages
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "OutgoingMessage.h"

#include "Client.h"
#include "Server.h"
#include "object/DataPoint.h"
#include "remote/Connection.h"
#include "remote/Helper.h"

using namespace Remote::Message;

OutgoingMessage::OutgoingMessage(const std::uint_fast16_t ca)
    : IMessageInterface(), sent(false), success(false) {
  commonAddress = ca;
}

OutgoingMessage::OutgoingMessage(std::shared_ptr<Object::DataPoint> point)
    : sent(false), success(false) {
  if (!point)
    throw std::invalid_argument("Cannot create OutgoingMessage without point");

  io = nullptr;

  type = point->getType();
  quality = point->getQuality();
  value = point->getValue();

  causeOfTransmission = CS101_COT_UNKNOWN_COT;

  auto _station = point->getStation();
  if (!_station) {
    throw std::invalid_argument("Cannot get station from point");
  }

  commonAddress = _station->getCommonAddress();

  if (_station->isLocal()) {
    auto _server = _station->getServer();
    if (!_server) {
      throw std::invalid_argument("Cannot get server from station");
    }
    server = _server;
  } else {
    auto _connection = _station->getConnection();
    if (!_connection) {
      throw std::invalid_argument("Cannot get connection from station");
    }
    connection = _connection;
    connectionString = _connection->getConnectionString();
  }

  informationObjectAddress = point->getInformationObjectAddress();
  DEBUG_PRINT(Debug::Message, "Created (outgoing)");
}

OutgoingMessage::~OutgoingMessage() {
  DEBUG_PRINT(Debug::Message, "Removed (outgoing)");
}

void OutgoingMessage::setOriginatorAddress(const std::uint_fast8_t address) {
  originatorAddress = address;
}

void OutgoingMessage::setCauseOfTransmission(
    const CS101_CauseOfTransmission cause) {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  causeOfTransmission = cause;
}

void OutgoingMessage::setIsPeriodicTransmission() {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  causeOfTransmission = CS101_COT_PERIODIC;
}

void OutgoingMessage::setIsSpontaneousTransmission() {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  causeOfTransmission = CS101_COT_SPONTANEOUS;
}

void OutgoingMessage::setIsRequestedTransmission() {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  causeOfTransmission = CS101_COT_REQUEST;
}

void OutgoingMessage::setQuality(const Quality descriptor) {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  quality.store(descriptor);
}

// IS_SENT

bool OutgoingMessage::getIsSent() const { return sent.load(); }

bool OutgoingMessage::send(IMasterConnection master) {
  auto _server = getServer();
  if (_server) {
    if (master) {
      auto param = IMasterConnection_getApplicationLayerParameters(master);
      originatorAddress = param->originatorAddress;
    }
    DEBUG_PRINT(Debug::Message, "OutgoingMessage.send] As Server: " +
                                    std::string(TypeID_toString(type)) +
                                    " | IOA " +
                                    std::to_string(informationObjectAddress));
    success = _server->send(shared_from_this(), master);
  } else {
    auto _connection = getConnection();
    if (!_connection) {
      throw std::invalid_argument("Server or connection reference deleted");
    }

    // originator is auto-injected by lib60870-c for commands, but add to
    // message object for convenience
    originatorAddress = _connection->getOriginatorAddress();
    DEBUG_PRINT(Debug::Message, "OutgoingMessage.send] As Client: " +
                                    std::string(TypeID_toString(type)) +
                                    " | IOA " +
                                    std::to_string(informationObjectAddress));
    success = _connection->command(shared_from_this());
  }
  sent = true;

  return success;
}

std::shared_ptr<Server> OutgoingMessage::getServer() {
  if (server.expired()) {
    return {nullptr};
  }
  return server.lock();
}

std::shared_ptr<Remote::Connection> OutgoingMessage::getConnection() {
  if (connection.expired()) {
    return {nullptr};
  }
  return connection.lock();
}
