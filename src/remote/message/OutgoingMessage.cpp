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
#include "object/DataPoint.h"
#include "object/DateTime.h"
#include "object/Station.h"
#include "object/information/Generic.h"
#include "transformer/Information.h"
#include "transformer/Type.h"

using namespace Remote::Message;

std::shared_ptr<OutgoingMessage>
OutgoingMessage::create(std::shared_ptr<Object::DataPoint> point) {
  // Not using std::make_shared because the constructor is private.
  return std::shared_ptr<OutgoingMessage>(
      new OutgoingMessage(std::move(point)));
}

OutgoingMessage::OutgoingMessage() : IMessageInterface() {
  DEBUG_PRINT(Debug::Message,
              "Created (outgoing) at " +
                  std::to_string(reinterpret_cast<std::uintptr_t>(this)));
}

OutgoingMessage::OutgoingMessage(
    const std::shared_ptr<Object::DataPoint> &point)
    : IMessageInterface() {
  if (!point)
    throw std::invalid_argument("Cannot create OutgoingMessage without point");

  io = nullptr;

  info = point->getInfo();
  type = Transformer::asType(
      info, true); // todo use COT to decide if with or without timestamp?

  causeOfTransmission = (info->getCategory() == COMMAND)
                            ? CS101_COT_ACTIVATION
                            : CS101_COT_SPONTANEOUS;

  const auto _station = point->getStation();
  if (!_station) {
    throw std::invalid_argument("Cannot get station from point");
  }

  commonAddress = _station->getCommonAddress();

  const auto now = Object::DateTime::now(_station, true);

  // updated locally processed timestamp before transmission
  point->setProcessedAt(now);

  if (point->getRecordedAt().has_value()) {
    // convert to station timezone and DST
    Object::DateTime dt{point->getRecordedAt().value()};
    dt.convertTimeZone(_station->getTimeZoneOffset(),
                       _station->isDaylightSavingTime());
    reported_at = dt;
  } else {
    // now is already in station timezone and DST
    reported_at = now;
  }

  informationObjectAddress = point->getInformationObjectAddress();

  if (std::dynamic_pointer_cast<Object::Information::Generic>(info)) {
    switch (type) {
      // End of initialization
    case M_EI_NA_1: {
      // todo remove??
      throw std::invalid_argument(
          "End of initialization is not a PointMessage!");
      informationObjectAddress = 0;
      io = (InformationObject)EndOfInitialization_create(
          nullptr, IEC60870_COI_REMOTE_RESET);
    } break;

    case M_SP_TA_1:
    case M_DP_TA_1:
    case M_ST_TA_1:
    case M_BO_TA_1:
    case M_ME_TA_1:
    case M_ME_TB_1:
    case M_ME_TC_1:
    case M_IT_TA_1:
    case M_EP_TA_1:
    case M_EP_TB_1:
    case M_EP_TC_1: {
      throw std::invalid_argument("CP24Time based messages "
                                  "not supported by norm IEC60870-5-104!");
    }

    default:
      throw std::invalid_argument("Unsupported type " +
                                  std::string(TypeID_toString(type)));
    }
  } else {
    // todo timestamp as argument
    io = Transformer::asInformationObject(info, informationObjectAddress, true);
  }

  DEBUG_PRINT(Debug::Message,
              "Created (outgoing) at " +
                  std::to_string(reinterpret_cast<std::uintptr_t>(this)));
}

OutgoingMessage::~OutgoingMessage() {
  if (io)
    InformationObject_destroy(io);

  DEBUG_PRINT(Debug::Message,
              "Removed (outgoing) at " +
                  std::to_string(reinterpret_cast<std::uintptr_t>(this)));
}

void OutgoingMessage::setOriginatorAddress(const std::uint_fast8_t address) {
  originatorAddress = address;
}

void OutgoingMessage::setCauseOfTransmission(
    const CS101_CauseOfTransmission cause) {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  causeOfTransmission = cause;
}
