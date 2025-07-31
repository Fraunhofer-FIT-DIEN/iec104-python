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
 * @file DataPoint.cpp
 * @brief 60870-5-104 information object
 *
 * @package iec104-python
 * @namespace Object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/DataPoint.h"
#include "Server.h"
#include "constants.h"
#include "module/ScopedGilAcquire.h"
#include "object/DateTime.h"
#include "object/Station.h"
#include "object/information/ICommand.h"
#include "object/information/IntegratedTotalInfo.h"
#include "remote/Connection.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"
#include <sstream>

using namespace Object;

std::shared_ptr<DataPoint>
DataPoint::create(const std::uint_fast32_t dp_ioa,
                  std::shared_ptr<Information::IInformation> dp_info,
                  const std::shared_ptr<Station> &dp_station,
                  const std::uint_fast16_t dp_report_ms,
                  const std::optional<std::uint_fast32_t> dp_related_ioa,
                  const bool dp_related_auto_return,
                  const CommandTransmissionMode dp_cmd_mode,
                  const std::uint_fast16_t tick_rate_ms) {
  Module::ScopedGilAcquire scoped("DataPoint.create");

  // Not using std::make_shared because the constructor is private.
  return std::shared_ptr<DataPoint>(
      new DataPoint(dp_ioa, dp_info, dp_station, dp_report_ms, dp_related_ioa,
                    dp_related_auto_return, dp_cmd_mode, tick_rate_ms));
}

DataPoint::DataPoint(const std::uint_fast32_t dp_ioa,
                     std::shared_ptr<Information::IInformation> dp_info,
                     const std::shared_ptr<Station> &dp_station,
                     const std::uint_fast16_t dp_report_ms,
                     const std::optional<std::uint_fast32_t> dp_related_ioa,
                     const bool dp_related_auto_return,
                     const CommandTransmissionMode dp_cmd_mode,
                     const std::uint_fast16_t tick_rate_ms)
    : informationObjectAddress(dp_ioa), station(dp_station),
      relatedInformationObjectAutoReturn(dp_related_auto_return),
      tickRate_ms(tick_rate_ms), timerNext(std::chrono::steady_clock::now()) {

  is_server = dp_station && dp_station->isLocal();

  // unsigned is always >= 0
  if (MAX_INFORMATION_OBJECT_ADDRESS < dp_ioa) {
    throw std::invalid_argument("Invalid information object address " +
                                std::to_string(dp_ioa));
  }

  setInfo(std::move(dp_info));
  setRelatedInformationObjectAddress(dp_related_ioa);

  if (info->getCategory() == COMMAND) {
    setCommandMode(dp_cmd_mode);
    setRelatedInformationObjectAutoReturn(dp_related_auto_return);
  }

  lastSentAt = std::chrono::steady_clock::now();
  setReportInterval_ms(dp_report_ms);

  DEBUG_PRINT(Debug::Point, "Created");
}

DataPoint::~DataPoint() { DEBUG_PRINT(Debug::Point, "Removed"); }

std::shared_ptr<Station> DataPoint::getStation() const {
  if (station.expired()) {
    return {nullptr};
  }
  return station.lock();
}

std::uint_fast32_t DataPoint::getInformationObjectAddress() const {
  return informationObjectAddress;
}

std::optional<std::uint_fast32_t>
DataPoint::getRelatedInformationObjectAddress() const {
  std::uint_fast32_t ioa = relatedInformationObjectAddress.load();
  if (MAX_INFORMATION_OBJECT_ADDRESS < ioa) {
    return std::nullopt;
  }
  return ioa;
}

void DataPoint::setRelatedInformationObjectAddress(
    const std::optional<std::uint_fast32_t> related_io_address) {
  if (related_io_address.has_value()) {
    if (MAX_INFORMATION_OBJECT_ADDRESS < related_io_address) {
      throw std::invalid_argument(
          "Invalid related information object address " +
          std::to_string(related_io_address.value()));
    }

    if (!is_server) {
      throw std::invalid_argument(
          "Related IO address option is only allowed for server-sided points");
    }
    relatedInformationObjectAddress.store(related_io_address.value());
  } else {
    relatedInformationObjectAddress.store(UNDEFINED_INFORMATION_OBJECT_ADDRESS);
  }
}

bool DataPoint::getRelatedInformationObjectAutoReturn() const {
  return relatedInformationObjectAutoReturn.load();
}

void DataPoint::setRelatedInformationObjectAutoReturn(const bool auto_return) {
  if (auto_return) {
    if (MAX_INFORMATION_OBJECT_ADDRESS <
        relatedInformationObjectAddress.load()) {
      throw std::invalid_argument("Related IO auto return option cannot be "
                                  "used without the related IO address option");
    }
    if (info->getCategory() != COMMAND) {
      throw std::invalid_argument(
          "Related IO auto return option is only "
          "allowed for control information, but not for " +
          info->name());
    }
  }
  relatedInformationObjectAutoReturn.store(auto_return);
}

CommandTransmissionMode DataPoint::getCommandMode() const {
  return commandMode.load();
}

void DataPoint::setCommandMode(const CommandTransmissionMode mode) {
  const auto cmd =
      std::dynamic_pointer_cast<Object::Information::ICommand>(info);
  if (!cmd) {
    throw std::invalid_argument(
        "Command mode can be set for commands, but not for " + info->name());
  }
  if (SELECT_AND_EXECUTE_COMMAND == mode && !cmd->isSelectable()) {
    throw std::invalid_argument("Select and Execute mode is not support for " +
                                cmd->name());
  }

  commandMode.store(mode);
}

std::optional<std::uint_fast8_t>
DataPoint::getSelectedByOriginatorAddress() const {
  if (const auto st = getStation()) {
    if (const auto server = st->getServer()) {
      return server->getSelector(st->getCommonAddress(),
                                 informationObjectAddress);
    }
  }
  return std::nullopt;
}

std::shared_ptr<Information::IInformation> DataPoint::getInfo() const {
  return info;
}

void DataPoint::setInfo(std::shared_ptr<Information::IInformation> new_info,
                        bool allow_all) {
  bool const debug = DEBUG_TEST(Debug::Point);

  if (info && !allow_all && info->name().compare(new_info->name()) != 0) {
    throw std::invalid_argument(
        "Incompatible information type (is: " + new_info->name() +
        ", expected: " + info->name() + ")");
  }
  if (!new_info->getRecordedAt().has_value()) {
    new_info->setRecordedAt(DateTime::now(getStation(), false));
    DEBUG_PRINT_CONDITION(debug, Debug::Point,
                          "Injecting current local timestamp into "
                          "information for " +
                              new_info->name() + " at IOA " +
                              std::to_string(informationObjectAddress));
  }

  info = std::move(new_info);
}

InfoValue DataPoint::getValue() const { return info->getValue(); }

void DataPoint::setValue(const InfoValue &new_value) const {
  info->setValue(new_value);
  info->setRecordedAt(DateTime::now(getStation(), false));
  DEBUG_PRINT(Debug::Point, "Injecting current local timestamp into " +
                                info->name() + " at IOA " +
                                std::to_string(informationObjectAddress));
}

InfoQuality DataPoint::getQuality() const { return info->getQuality(); }

void DataPoint::setQuality(const InfoQuality new_quality) const {
  info->setQuality(new_quality);
  info->setRecordedAt(DateTime::now(getStation(), false));
  DEBUG_PRINT(Debug::Point, "Injecting current local timestamp into " +
                                info->name() + " at IOA " +
                                std::to_string(informationObjectAddress));
}

std::optional<DateTime> DataPoint::getRecordedAt() const {
  return info->getRecordedAt();
}

DateTime DataPoint::getProcessedAt() const { return info->getProcessedAt(); }

void DataPoint::setProcessedAt(const DateTime &val) {
  lastSentAt.store(std::chrono::steady_clock::now());
  info->setProcessedAt(val);
}

std::uint_fast16_t DataPoint::getReportInterval_ms() const {
  return reportInterval_ms.load();
}

void DataPoint::setReportInterval_ms(const std::uint_fast16_t interval_ms) {
  if (interval_ms > 0) {
    if (interval_ms < tickRate_ms || interval_ms % tickRate_ms != 0) {
      throw std::range_error("interval_ms (=" + std::to_string(interval_ms) +
                             ") must be a positive integer multiple "
                             "of server/client tickRate_ms (=" +
                             std::to_string(tickRate_ms) + ")");
    }

    // only monitoring status points
    if (info->getCategory() != MONITORING_STATUS) {
      throw std::invalid_argument("interval_ms option is not supported for " +
                                  info->name());
    }
    if (!is_server) {
      throw std::invalid_argument(
          "Report interval option is only allowed for server-sided points");
    }
  }
  reportInterval_ms.store(interval_ms);
}

std::uint_fast16_t DataPoint::getTimerInterval_ms() const {
  return timerInterval_ms.load();
}

void DataPoint::setOnReceiveCallback(py::object &callable) {
  py_onReceive.reset(callable);
}

CommandResponseState DataPoint::onReceive(
    const std::shared_ptr<Remote::Message::IncomingMessage> &message) {
  auto prev = std::move(info);
  info = message->getInfo();

  if (py_onReceive.is_set()) {
    DEBUG_PRINT(Debug::Point, "CALLBACK on_receive at IOA " +
                                  std::to_string(informationObjectAddress));
    Module::ScopedGilAcquire const scoped("Point.on_receive");

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
  if (!is_server) {
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
  if (!is_server) {
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

std::optional<std::chrono::steady_clock::time_point>
DataPoint::nextReportAt() const {
  if (reportInterval_ms > 0) {
    return lastSentAt.load() + std::chrono::milliseconds(reportInterval_ms);
  }
  return std::nullopt;
}

std::optional<std::chrono::steady_clock::time_point>
DataPoint::nextTimerAt() const {
  if (timerInterval_ms > 0 && py_onTimer.is_set()) {
    return timerNext;
  }
  return std::nullopt;
}

std::list<size_t> DataPoint::getGroups() {
  const auto _station = getStation();
  if (_station) {
    return _station->getGroupsForDataPoint(shared_from_this());
  }

  return {};
}

void DataPoint::setGroups(const std::list<size_t> &groups) {
  const auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Station reference deleted");
  }
  // as client
  if (!_station->isLocal()) {
    throw std::invalid_argument("Cannot assign groups as client");
  }

  // Test restriction for counter values
  if (std::dynamic_pointer_cast<Information::BinaryCounterInfo>(info) !=
      nullptr) {
    for (const size_t index : groups) {
      if (index > 4) {
        throw std::invalid_argument(
            "Point.groups] Integrated total points may use group 1 to 4, but " +
            std::to_string(index) + " given");
      }
    }
  }

  // Add point to specified new groups
  _station->setGroupsForDataPoint(shared_from_this(), groups);
}

void DataPoint::setOnTimerCallback(py::object &callable,
                                   const std::uint_fast16_t interval_ms) {
  if (interval_ms < tickRate_ms || interval_ms % tickRate_ms != 0)
    throw std::range_error("interval_ms must be a positive integer multiple of "
                           "server/client tickRate_ms");
  timerInterval_ms.store(callable.is_none() ? 0 : interval_ms);
  py_onTimer.reset(callable);
}

void DataPoint::onTimer() {
  if (py_onTimer.is_set()) {
    timerNext = std::chrono::steady_clock::now() +
                std::chrono::milliseconds(timerInterval_ms);
    DEBUG_PRINT(Debug::Point, "CALLBACK on_timer at IOA " +
                                  std::to_string(informationObjectAddress));
    Module::ScopedGilAcquire scoped("Point.on_timer");
    py_onTimer.call(shared_from_this());
  }
}

bool DataPoint::read() {
  const auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Station reference deleted");
  }

  // as server
  if (_station->isLocal()) {
    throw std::invalid_argument("Cannot send read commands as server");
  }

  // as client
  const auto _connection = _station->getConnection();
  if (!_connection) {
    throw std::invalid_argument("Connection reference deleted");
  }

  return _connection->read(shared_from_this());
}

bool DataPoint::transmit(const CS101_CauseOfTransmission cause,
                         const bool timestamp) {
  DEBUG_PRINT(Debug::Point, "transmit_ex] " + info->name() + " at IOA " +
                                std::to_string(informationObjectAddress));

  const auto _station = getStation();
  if (!_station) {
    throw std::invalid_argument("Station reference deleted");
  }

  // as server
  if (_station->isLocal()) {
    const auto server = _station->getServer();
    if (!server) {
      throw std::invalid_argument("Server reference deleted");
    }
    return server->transmit(shared_from_this(), cause, timestamp);
  }

  // as client
  const auto connection = _station->getConnection();
  if (!connection) {
    throw std::invalid_argument("Client connection reference deleted");
  }
  return connection->transmit(shared_from_this(), cause);
}

void DataPoint::detach() { station.reset(); }

std::string DataPoint::toString() const {
  std::ostringstream oss;
  oss << "<c104.Point io_address=" << std::to_string(informationObjectAddress)
      << ", info=" << info->name()
      << ", report_ms=" << std::to_string(reportInterval_ms.load())
      << ", related_io_address=";

  if (relatedInformationObjectAddress < MAX_INFORMATION_OBJECT_ADDRESS)
    oss << std::to_string(relatedInformationObjectAddress.load());
  else
    oss << "None";

  oss << ", related_io_autoreturn="
      << bool_toString(relatedInformationObjectAutoReturn.load())
      << ", command_mode=" << CommandTransmissionMode_toString(commandMode)
      << " at " << std::hex << std::showbase
      << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
}
