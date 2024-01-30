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

OutgoingMessage::OutgoingMessage(std::shared_ptr<Object::DataPoint> point)
    : IMessageInterface() {
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
