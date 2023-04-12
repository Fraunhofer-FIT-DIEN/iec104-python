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
 * @file DataPoint.cpp
 * @brief 60870-5-104 information object
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/DataPoint.h"
#include "module/ScopedGilAcquire.h"
#include "object/Station.h"
#include "remote/Connection.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/PointCommand.h"
#include "remote/message/PointMessage.h"

using namespace Object;

DataPoint::DataPoint(const std::uint_fast32_t dp_ioa,
                     const IEC60870_5_TypeID dp_type,
                     std::shared_ptr<Station> dp_station,
                     const std::uint_fast32_t dp_report_ms,
                     const std::uint_fast32_t dp_related_ioa,
                     const bool dp_related_auto_return)
    : informationObjectAddress(dp_ioa), type(dp_type), station(dp_station),
      reportInterval_ms(dp_report_ms),
      relatedInformationObjectAddress(dp_related_ioa),
      relatedInformationObjectAutoReturn(dp_related_auto_return) {
  if (type >= M_EI_NA_1) {
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }
  DEBUG_PRINT(Debug::Point, "Created");
}

DataPoint::~DataPoint() { DEBUG_PRINT(Debug::Point, "Removed"); }

std::shared_ptr<Station> DataPoint::getStation() {
  if (station.expired()) {
    return {nullptr};
  }
  return station.lock();
}

std::uint_fast32_t DataPoint::getInformationObjectAddress() const {
  return informationObjectAddress;
}

std::uint_fast32_t DataPoint::getRelatedInformationObjectAddress() const {
  return relatedInformationObjectAddress.load();
}

void DataPoint::setRelatedInformationObjectAddress(
    const std::uint_fast32_t related_io_address) {
  relatedInformationObjectAddress.store(related_io_address);
}

bool DataPoint::getRelatedInformationObjectAutoReturn() const {
  return relatedInformationObjectAutoReturn.load();
}

void DataPoint::setRelatedInformationObjectAutoReturn(const bool auto_return) {
  relatedInformationObjectAutoReturn.store(auto_return);
}

IEC60870_5_TypeID DataPoint::getType() const { return type; }

Quality DataPoint::getQuality() const { return quality.load(); }

void DataPoint::setQuality(const Quality &new_quality) {
  Quality const prev_quality = quality.load();
  if (prev_quality != new_quality) {
    quality.store(new_quality);
    DEBUG_PRINT(Debug::Point,
                "set_quality] prev: " + Quality_toString(prev_quality) +
                    ") new: " + Quality_toString(new_quality) + " at IOA " +
                    std::to_string(informationObjectAddress));
  }
}

double DataPoint::getValue() const { return value; }

std::int32_t DataPoint::getValueAsInt32() const { return (int)value; }

float DataPoint::getValueAsFloat() const { return (float)value; }

std::uint32_t DataPoint::getValueAsUInt32() const { return (uint32_t)value; }

void DataPoint::setValue(const double new_value) {
  setValueEx(new_value, Quality::None, 0);
}

void DataPoint::setValueEx(const double new_value, const Quality &new_quality,
                           const std::uint_fast64_t timestamp_ms) {
  // set predefined timestamp if provided (as client)
  if (timestamp_ms > 0) {
    updatedAt_ms = timestamp_ms;
  } else {
    updatedAt_ms = GetTimestamp_ms();
  }

  // detect NaN values
  if (std::isnan(new_value)) {
    // quality.store(Quality::Invalid);
    DEBUG_PRINT(Debug::Point, "set_value_ex] detected NaN value at IOA " +
                                  std::to_string(informationObjectAddress));
    // return;
  }

  double const val = value.load();
  Quality const qval = quality.load();
  if (val != new_value) {
    value.store(new_value);
  }
  quality.store(new_quality);

  if (is_none(new_quality)) {
    switch (type) {
    case M_SP_NA_1:
    case M_SP_TA_1:
    case M_SP_TB_1:
    case C_SC_NA_1:
    case C_SC_TA_1: {
      if (new_value != 0 && new_value != 1) {
        // unexpected bad quality
        std::cerr << "[c104.Point.setValueEx] Cannot set value of M_SP and "
                     "C_SC to numbers other than 0 and 1 at IOA "
                  << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_DP_NA_1:
    case M_DP_TA_1:
    case M_DP_TB_1: {
      if (new_value != 0 && new_value != 1 && new_value != 2 &&
          new_value != 3) {
        // unexpected bad quality
        std::cerr << "[c104.Point.setValueEx] Cannot set value of M_DP to "
                     "numbers other than 0,1,2,3 at IOA "
                  << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case C_DC_NA_1:
    case C_DC_TA_1:
    case C_RC_NA_1:
    case C_RC_TA_1: {
      if (new_value != 1 && new_value != 2) {
        // unexpected bad quality
        std::cerr << "[c104.Point.setValueEx] Cannot set value of C_DC and "
                     "C_RC to numbers other than 1,2 at IOA "
                  << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_ST_NA_1:
    case M_ST_TA_1:
    case M_ST_TB_1: {
      double int_part = 0;
      if (std::modf(new_value, &int_part) != 0.0 || new_value < -63 ||
          new_value > 64) {
        // unexpected bad quality
        std::cerr << "[c104.Point.setValueEx] Cannot set value of M_ST to "
                     "numbers other than [-63, ... , +64] at IOA "
                  << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_BO_NA_1:
    case M_BO_TA_1:
    case M_BO_TB_1:
    case C_BO_NA_1:
    case C_BO_TA_1: {
      double int_part = 0;
      if (std::modf(new_value, &int_part) != 0.0 || new_value < 0 ||
          new_value >= std::pow(2, 32)) {
        // unexpected bad quality
        std::cerr << "[c104.Point.setValueEx] Cannot set value of M_BO and "
                     "C_BO to numbers other than [0, ... , 2^32 - 1] at IOA "
                  << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_ME_NA_1:
    case M_ME_ND_1:
    case M_ME_TA_1:
    case M_ME_TD_1:
    case C_SE_NA_1:
    case C_SE_TA_1: {
      if (new_value < -1.f || new_value > 1.f) {
        // unexpected bad quality
        std::cerr
            << "[c104.Point.setValueEx] Cannot set value of M_ME (normalized) "
               "to numbers other than [-1.0, ... , +1.0] at IOA "
            << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_ME_NB_1:
    case M_ME_TB_1:
    case M_ME_TE_1:
    case C_SE_NB_1:
    case C_SE_TB_1: {
      if (new_value < -65536. || new_value > 65535.) {
        // unexpected bad quality
        std::cerr
            << "[c104.Point.setValueEx] Cannot set value of M_ME (scaled) to "
               "numbers other than [-2^16, ... , +2^16 - 1] at IOA "
            << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_ME_NC_1:
    case M_ME_TC_1:
    case M_ME_TF_1:
    case C_SE_NC_1:
    case C_SE_TC_1: {
      if (new_value < -16777216. || new_value > 16777215.) {
        // unexpected bad quality
        std::cerr << "[c104.Point.setValueEx] Cannot set value of M_ME (short) "
                     "to numbers other than [-2^24, ... , +2^24 - 1] at IOA "
                  << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    case M_IT_NA_1:
    case M_IT_TA_1:
    case M_IT_TB_1: {
      double int_part = 0;
      if (std::modf(new_value, &int_part) != 0.0 || new_value < -65536 ||
          new_value > 65535) {
        // unexpected bad quality
        std::cerr
            << "[c104.Point.setValueEx] Cannot set value of M_IT to numbers "
               "other than [-2^16, ... , +2^16 - 1] (4x uint8) at IOA "
            << std::to_string(informationObjectAddress) << std::endl;
        quality.store(Quality::Invalid);
      }
    } break;
    }
  }
  DEBUG_PRINT(Debug::Point, "set_value_ex] prev: " + std::to_string(val) +
                                " (" + Quality_toString(qval) + ") new: " +
                                std::to_string(value.load()) + " (" +
                                Quality_toString(new_quality) + ") at IOA " +
                                std::to_string(informationObjectAddress));
}

uint64_t DataPoint::getUpdatedAt_ms() const { return updatedAt_ms.load(); }

uint64_t DataPoint::getReportedAt_ms() const { return reportedAt_ms.load(); }

void DataPoint::setReportedAt_ms(const std::uint_fast64_t timestamp_ms) {
  return reportedAt_ms.store(timestamp_ms);
}

std::uint_fast32_t DataPoint::getReportInterval_ms() const {
  return reportInterval_ms.load();
}

void DataPoint::setReportInterval_ms(const std::uint_fast32_t interval_ms) {
  reportInterval_ms.store(interval_ms);
}

uint64_t DataPoint::getReceivedAt_ms() const { return receivedAt_ms.load(); }

uint64_t DataPoint::getSentAt_ms() const { return sentAt_ms.load(); }

void DataPoint::setOnReceiveCallback(py::object &callable) {
  py_onReceive.reset(callable);
}

ResponseState DataPoint::onReceive(
    std::shared_ptr<Remote::Message::IncomingMessage> message) {
  double const prev_value = value.load();
  Quality const prev_quality = quality.load();
  uint64_t const prev_updatedAt = updatedAt_ms.load();
  setValueEx(message->getValue(), message->getQuality(),
             message->getUpdatedAt());
  receivedAt_ms = GetTimestamp_ms();

  if (py_onReceive.is_set()) {
    DEBUG_PRINT(Debug::Point, "CALLBACK on_receive at IOA " +
                                  std::to_string(informationObjectAddress));
    Module::ScopedGilAcquire const scoped("Point.on_receive");

    py::dict prev;
    prev["value"] = prev_value;
    prev["quality"] = prev_quality;
    prev["updatedAt_ms"] = prev_updatedAt;

    if (py_onReceive.call(shared_from_this(), prev, message)) {
      try {
        return py_onReceive.getResult();
      } catch (const std::exception &e) {
        DEBUG_PRINT(Debug::Point, "on_receive] Invalid callback result: " +
                                      std::string(e.what()));
        return RESPONSE_STATE_FAILURE;
      }
    }
  }

  return RESPONSE_STATE_SUCCESS;
}

void DataPoint::setOnBeforeReadCallback(py::object &callable) {
  auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Station reference deleted");
  }
  if (!_station->isLocal()) {
    throw std::invalid_argument("Cannot set callback as client");
  }
  py_onBeforeRead.reset(callable);
}

void DataPoint::onBeforeRead() {
  if (py_onBeforeRead.is_set()) {
    DEBUG_PRINT(Debug::Point, "CALLBACK on_before_read at IOA " +
                                  std::to_string(informationObjectAddress));
    Module::ScopedGilAcquire scoped("Point.on_before_read");
    py_onBeforeRead.call(shared_from_this());
  }
}

void DataPoint::setOnBeforeAutoTransmitCallback(py::object &callable) {
  auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Station reference deleted");
  }
  if (!_station->isLocal()) {
    throw std::invalid_argument("Cannot set callback as client");
  }
  py_onBeforeAutoTransmit.reset(callable);
}

void DataPoint::onBeforeAutoTransmit() {
  if (py_onBeforeAutoTransmit.is_set()) {
    DEBUG_PRINT(Debug::Point, "CALLBACK on_before_auto_transmit at IOA " +
                                  std::to_string(informationObjectAddress));
    Module::ScopedGilAcquire scoped("Point.on_before_auto_transmit");
    py_onBeforeAutoTransmit.call(shared_from_this());
  }
}

bool DataPoint::read() {
  auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Station reference deleted");
  }

  // as server
  if (_station->isLocal()) {
    throw std::invalid_argument("Cannot send read commands as server");
  }

  // as client
  auto _connection = _station->getConnection();
  if (!_connection) {
    throw std::invalid_argument("Connection reference deleted");
  }

  return _connection->read(shared_from_this());
}

bool DataPoint::transmit(const CS101_CauseOfTransmission cause) {
  return transmitEx(cause, nullptr);
}

bool DataPoint::transmitEx(const CS101_CauseOfTransmission cause,
                           IMasterConnection master) {
  DEBUG_PRINT(Debug::Point,
              "transmit_ex] " + std::string(TypeID_toString(type)) +
                  " at IOA " + std::to_string(informationObjectAddress));

  auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Cannot get station from point");
  }

  // as server
  if (_station->isLocal()) {
    sentAt_ms = GetTimestamp_ms();

    // lazy confirm previous command
    if (cause == CS101_COT_ACTIVATION_CON ||
        cause == CS101_COT_DEACTIVATION_CON ||
        cause == CS101_COT_ACTIVATION_TERMINATION) {

      // manual command confirmation to client (modbus extension support)
      auto message = Remote::Message::PointCommand::create(shared_from_this());
      message->setCauseOfTransmission(cause);
      return message->send();

    } else {

      // transmit value to client
      auto message = Remote::Message::PointMessage::create(shared_from_this());
      message->setCauseOfTransmission(cause);
      return message->send(master);
    }
  }

  // as client
  sentAt_ms = GetTimestamp_ms();

  // transmit value to server
  auto message = Remote::Message::PointCommand::create(shared_from_this());
  message->setCauseOfTransmission(cause);
  return message->send();
}
