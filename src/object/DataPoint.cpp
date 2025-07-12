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
#include "module/ScopedGilAcquire.h"
#include "object/DateTime.h"
#include "object/Information.h"
#include "object/Station.h"
#include "remote/Connection.h"
#include "remote/message/IncomingMessage.h"

using namespace Object;

DataPoint::DataPoint(const std::uint_fast32_t dp_ioa,
                     const IEC60870_5_TypeID dp_type,
                     const std::shared_ptr<Station> &dp_station,
                     const std::uint_fast16_t dp_report_ms,
                     const std::optional<std::uint_fast32_t> dp_related_ioa,
                     const bool dp_related_auto_return,
                     const CommandTransmissionMode dp_cmd_mode,
                     const std::uint_fast16_t tick_rate_ms)
    : informationObjectAddress(dp_ioa), type(dp_type), station(dp_station),
      relatedInformationObjectAutoReturn(dp_related_auto_return),
      commandMode(dp_cmd_mode), tickRate_ms(tick_rate_ms),
      timerNext(std::chrono::steady_clock::now()) {
  if (type >= M_EI_NA_1) {
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }

  is_server = dp_station && dp_station->isLocal();

  // unsigned is always >= 0
  if (MAX_INFORMATION_OBJECT_ADDRESS < dp_ioa) {
    throw std::invalid_argument("Invalid information object address " +
                                std::to_string(dp_ioa));
  }

  lastSentAt = std::chrono::steady_clock::now();
  setReportInterval_ms(dp_report_ms);

  if (dp_related_ioa.has_value()) {
    if (MAX_INFORMATION_OBJECT_ADDRESS < dp_related_ioa) {
      throw std::invalid_argument(
          "Invalid related information object address " +
          std::to_string(dp_related_ioa.value()));
    }
    relatedInformationObjectAddress = dp_related_ioa.value();
    if (!is_server) {
      throw std::invalid_argument(
          "Related IO address option is only allowed for server-sided points");
    }
  }
  if (relatedInformationObjectAutoReturn) {
    if (MAX_INFORMATION_OBJECT_ADDRESS < relatedInformationObjectAddress) {
      throw std::invalid_argument("Related IO auto return option cannot be "
                                  "used without the related IO address option");
    }
    if (dp_type < C_SC_NA_1 || dp_type > C_BO_TA_1) {
      throw std::invalid_argument("Related IO auto return option is only "
                                  "allowed for control types, but not for " +
                                  std::string(TypeID_toString(type)));
    }
  }

  switch (type) {
  case M_SP_NA_1:
    info =
        std::make_shared<SingleInfo>(false, Quality::None, std::nullopt, false);
    break;
  case M_SP_TB_1:
    info = std::make_shared<SingleInfo>(
        false, Quality::None, DateTime::now(getStation(), false), false);
    break;
  case C_SC_NA_1:
    info = std::make_shared<SingleCmd>(
        false, false, CS101_QualifierOfCommand::NONE, std::nullopt, false);
    break;
  case C_SC_TA_1:
    info = std::make_shared<SingleCmd>(
        false, false, CS101_QualifierOfCommand::NONE,
        DateTime::now(getStation(), false), false);
    break;
  case M_DP_NA_1:
    info = std::make_shared<DoubleInfo>(IEC60870_DOUBLE_POINT_OFF,
                                        Quality::None, std::nullopt, false);
    break;
  case M_DP_TB_1:
    info =
        std::make_shared<DoubleInfo>(IEC60870_DOUBLE_POINT_OFF, Quality::None,
                                     DateTime::now(getStation(), false), false);
    break;
  case C_DC_NA_1:
    info = std::make_shared<DoubleCmd>(IEC60870_DOUBLE_POINT_OFF, false,
                                       CS101_QualifierOfCommand::NONE,
                                       std::nullopt, false);
  case C_DC_TA_1:
    info = std::make_shared<DoubleCmd>(
        IEC60870_DOUBLE_POINT_OFF, false, CS101_QualifierOfCommand::NONE,
        DateTime::now(getStation(), false), false);
    break;
  case M_ST_NA_1:
    info = std::make_shared<StepInfo>(LimitedInt7(0), false, Quality::None,
                                      std::nullopt, false);
    break;
  case M_ST_TB_1:
    info =
        std::make_shared<StepInfo>(LimitedInt7(0), false, Quality::None,
                                   DateTime::now(getStation(), false), false);
    break;
  case C_RC_NA_1:
    info = std::make_shared<StepCmd>(IEC60870_STEP_LOWER, false,
                                     CS101_QualifierOfCommand::NONE,
                                     std::nullopt, false);
    break;
  case C_RC_TA_1:
    info = std::make_shared<StepCmd>(IEC60870_STEP_LOWER, false,
                                     CS101_QualifierOfCommand::NONE,
                                     DateTime::now(getStation(), false), false);
    break;
  case M_ME_NA_1:
  case M_ME_ND_1:
    info = std::make_shared<NormalizedInfo>(NormalizedFloat(0), Quality::None,
                                            std::nullopt, false);
    break;
  case M_ME_TD_1:
    info = std::make_shared<NormalizedInfo>(NormalizedFloat(0), Quality::None,
                                            DateTime::now(getStation(), false),
                                            false);
    break;
  case C_SE_NA_1:
    info = std::make_shared<NormalizedCmd>(
        NormalizedFloat(0), false, LimitedUInt7(0), std::nullopt, false);
    break;
  case C_SE_TA_1:
    info = std::make_shared<NormalizedCmd>(
        NormalizedFloat(0), false, LimitedUInt7(0),
        DateTime::now(getStation(), false), false);
    break;
  case M_ME_NB_1:
    info = std::make_shared<ScaledInfo>(LimitedInt16(0), Quality::None,
                                        std::nullopt, false);
    break;
  case M_ME_TE_1:
    info =
        std::make_shared<ScaledInfo>(LimitedInt16(0), Quality::None,
                                     DateTime::now(getStation(), false), false);
    break;
  case C_SE_NB_1:
    info = std::make_shared<ScaledCmd>(LimitedInt16(0), false, LimitedUInt7(0),
                                       std::nullopt, false);
    break;
  case C_SE_TB_1:
    info =
        std::make_shared<ScaledCmd>(LimitedInt16(0), false, LimitedUInt7(0),
                                    DateTime::now(getStation(), false), false);
    break;
  case M_ME_NC_1:
    info = std::make_shared<ShortInfo>(0.0, Quality::None, std::nullopt, false);
    break;
  case M_ME_TF_1:
    info = std::make_shared<ShortInfo>(
        0.0, Quality::None, DateTime::now(getStation(), false), false);
    break;
  case C_SE_NC_1:
    info = std::make_shared<ShortCmd>(0.0, false, LimitedUInt7(0), std::nullopt,
                                      false);
    break;
  case C_SE_TC_1:
    info = std::make_shared<ShortCmd>(
        0.0, false, LimitedUInt7(0), DateTime::now(getStation(), false), false);
    break;
  case M_BO_NA_1:
    info = std::make_shared<BinaryInfo>(Byte32(0), Quality::None, std::nullopt,
                                        false);
    break;
  case M_BO_TB_1:
    info = std::make_shared<BinaryInfo>(
        Byte32(0), Quality::None, DateTime::now(getStation(), false), false);
    break;
  case C_BO_NA_1:
    info = std::make_shared<BinaryCmd>(Byte32(0), std::nullopt, false);
    break;
  case C_BO_TA_1:
    info = std::make_shared<BinaryCmd>(
        Byte32(0), DateTime::now(getStation(), false), false);
    break;
  case M_IT_NA_1:
    info = std::make_shared<BinaryCounterInfo>(
        0, LimitedUInt5(0), BinaryCounterQuality::None, std::nullopt, false);
    break;
  case M_IT_TB_1:
    info = std::make_shared<BinaryCounterInfo>(
        0, LimitedUInt5(0), BinaryCounterQuality::None,
        DateTime::now(getStation(), false), false);
    break;
  case M_EP_TD_1:
    info = std::make_shared<ProtectionEquipmentEventInfo>(
        IEC60870_EVENTSTATE_OFF, LimitedUInt16(0), Quality::None,
        DateTime::now(getStation(), false), false);
    break;
  case M_EP_TE_1:
    info = std::make_shared<ProtectionEquipmentStartEventsInfo>(
        StartEvents::None, LimitedUInt16(0), Quality::None,
        DateTime::now(getStation(), false), false);
    break;
  case M_EP_TF_1:
    info = std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        OutputCircuits::None, LimitedUInt16(0), Quality::None,
        DateTime::now(getStation(), false), false);
    break;
  case M_PS_NA_1:
    info = std::make_shared<StatusWithChangeDetection>(
        static_cast<FieldSet16>(0), static_cast<FieldSet16>(0), Quality::None,
        std::nullopt, false);
    break;
  default:
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }

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
    if (type < C_SC_NA_1 || type > C_BO_TA_1) {
      throw std::invalid_argument("Related IO auto return option is only "
                                  "allowed for control types, but not for " +
                                  std::string(TypeID_toString(type)));
    }
  }
  relatedInformationObjectAutoReturn.store(auto_return);
}

CommandTransmissionMode DataPoint::getCommandMode() const {
  return commandMode.load();
}

void DataPoint::setCommandMode(const CommandTransmissionMode mode) {
  if (SELECT_AND_EXECUTE_COMMAND == mode &&
      (type < C_SC_NA_1 || C_BO_NA_1 == type || C_SE_TC_1 < type)) {
    throw std::invalid_argument("Only control points, except for C_BO_* "
                                "support select and execute mode");
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

IEC60870_5_TypeID DataPoint::getType() const { return type; }

std::shared_ptr<Information> DataPoint::getInfo() const { return info; }

void DataPoint::setInfo(std::shared_ptr<Information> new_info) {
  bool const debug = DEBUG_TEST(Debug::Point);

  switch (type) {
  case M_SP_NA_1: {
    if (auto i = std::dynamic_pointer_cast<SingleInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type SingleInfo, but is " +
          new_info->name());
    }
  } break;
  case M_SP_TB_1: {
    if (auto i = std::dynamic_pointer_cast<SingleInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type SingleInfo, but is " +
          new_info->name());
    }
  } break;
  case C_SC_NA_1: {
    if (auto i = std::dynamic_pointer_cast<SingleCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type SingleCmd, but is " +
          new_info->name());
    }
  } break;
  case C_SC_TA_1: {
    if (auto i = std::dynamic_pointer_cast<SingleCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type SingleCmd, but is " +
          new_info->name());
    }
  } break;
  case M_DP_NA_1: {
    if (auto i = std::dynamic_pointer_cast<DoubleInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type DoubleInfo, but is " +
          new_info->name());
    }
  } break;
  case M_DP_TB_1: {
    if (auto i = std::dynamic_pointer_cast<DoubleInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type DoubleInfo, but is " +
          new_info->name());
    }
  } break;
  case C_DC_NA_1: {
    if (auto i = std::dynamic_pointer_cast<DoubleCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type DoubleCmd, but is " +
          new_info->name());
    }
  } break;
  case C_DC_TA_1: {
    if (auto i = std::dynamic_pointer_cast<DoubleCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type DoubleCmd, but is " +
          new_info->name());
    }
  } break;
  case M_ST_NA_1: {
    if (auto i = std::dynamic_pointer_cast<StepInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type StepInfo, but is " +
          new_info->name());
    }
  } break;
  case M_ST_TB_1: {
    if (auto i = std::dynamic_pointer_cast<StepInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type StepInfo, but is " +
          new_info->name());
    }
  } break;
  case C_RC_NA_1: {
    if (auto i = std::dynamic_pointer_cast<StepCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type StepCmd, but is " + new_info->name());
    }
  } break;
  case C_RC_TA_1: {
    if (auto i = std::dynamic_pointer_cast<StepCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type StepCmd, but is " + new_info->name());
    }
  } break;
  case M_ME_NA_1:
  case M_ME_ND_1: {
    if (auto i = std::dynamic_pointer_cast<NormalizedInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type NormalizedInfo, but is " +
          new_info->name());
    }
  } break;
  case M_ME_TD_1: {
    if (auto i = std::dynamic_pointer_cast<NormalizedInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type NormalizedInfo, but is " +
          new_info->name());
    }
  } break;
  case C_SE_NA_1: {
    if (auto i = std::dynamic_pointer_cast<NormalizedCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type NormalizedCmd, but is " +
          new_info->name());
    }
  } break;
  case C_SE_TA_1: {
    if (auto i = std::dynamic_pointer_cast<NormalizedCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type NormalizedCmd, but is " +
          new_info->name());
    }
  } break;
  case M_ME_NB_1: {
    if (auto i = std::dynamic_pointer_cast<ScaledInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ScaledInfo, but is " +
          new_info->name());
    }
  } break;
  case M_ME_TE_1: {
    if (auto i = std::dynamic_pointer_cast<ScaledInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ScaledInfo, but is " +
          new_info->name());
    }
  } break;
  case C_SE_NB_1: {
    if (auto i = std::dynamic_pointer_cast<ScaledCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ScaledCmd, but is " +
          new_info->name());
    }
  } break;
  case C_SE_TB_1: {
    if (auto i = std::dynamic_pointer_cast<ScaledCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ScaledCmd, but is " +
          new_info->name());
    }
  } break;
  case M_ME_NC_1: {
    if (auto i = std::dynamic_pointer_cast<ShortInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ShortInfo, but is " +
          new_info->name());
    }
  } break;
  case M_ME_TF_1: {
    if (auto i = std::dynamic_pointer_cast<ShortInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ShortInfo, but is " +
          new_info->name());
    }
  } break;
  case C_SE_NC_1: {
    if (auto i = std::dynamic_pointer_cast<ShortCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ShortCmd, but is " +
          new_info->name());
    }
  } break;
  case C_SE_TC_1: {
    if (auto i = std::dynamic_pointer_cast<ShortCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ShortCmd, but is " +
          new_info->name());
    }
  } break;
  case M_BO_NA_1: {
    if (auto i = std::dynamic_pointer_cast<BinaryInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type BinaryInfo, but is " +
          new_info->name());
    }
  } break;
  case M_BO_TB_1: {
    if (auto i = std::dynamic_pointer_cast<BinaryInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type BinaryInfo, but is " +
          new_info->name());
    }
  } break;
  case C_BO_NA_1: {
    if (auto i = std::dynamic_pointer_cast<BinaryCmd>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type BinaryCmd, but is " +
          new_info->name());
    }
  } break;
  case C_BO_TA_1: {
    if (auto i = std::dynamic_pointer_cast<BinaryCmd>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type BinaryCmd, but is " +
          new_info->name());
    }
  } break;
  case M_IT_NA_1: {
    if (auto i = std::dynamic_pointer_cast<BinaryCounterInfo>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type BinaryCounterInfo, but is " +
          new_info->name());
    }
  } break;
  case M_IT_TB_1: {
    if (auto i = std::dynamic_pointer_cast<BinaryCounterInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type BinaryCounterInfo, but is " +
          new_info->name());
    }
  } break;
  case M_EP_TD_1: {
    if (auto i =
            std::dynamic_pointer_cast<ProtectionEquipmentEventInfo>(new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ProtectionEventInfo, but is " +
          new_info->name());
    }
  } break;
  case M_EP_TE_1: {
    if (auto i = std::dynamic_pointer_cast<ProtectionEquipmentStartEventsInfo>(
            new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ProtectionStartInfo, but is " +
          new_info->name());
    }
  } break;
  case M_EP_TF_1: {
    if (auto i =
            std::dynamic_pointer_cast<ProtectionEquipmentOutputCircuitInfo>(
                new_info)) {
      if (!i->getRecordedAt().has_value()) {
        i->setRecordedAt(DateTime::now(getStation(), false));
        DEBUG_PRINT_CONDITION(debug, Debug::Point,
                              "Injecting current local timestamp into "
                              "information for [c104.Type." +
                                  std::string(TypeID_toString(type)) +
                                  "] at IOA " +
                                  std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type ProtectionCircuitInfo, but is " +
          new_info->name());
    }
  } break;
  case M_PS_NA_1: {
    if (auto i =
            std::dynamic_pointer_cast<StatusWithChangeDetection>(new_info)) {
      if (i->getRecordedAt().has_value()) {
        i->setRecordedAt(std::nullopt);
        DEBUG_PRINT_CONDITION(
            debug, Debug::Point,
            "Dropping timestamp of information for [c104.Type." +
                std::string(TypeID_toString(type)) + "] at IOA " +
                std::to_string(informationObjectAddress));
      }
    } else {
      throw std::invalid_argument(
          "[c104.Type." + std::string(TypeID_toString(type)) +
          "] requires Information of type StatusAndChanged, but is " +
          new_info->name());
    }
  } break;
  default:
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }
  info = std::move(new_info);
}

InfoValue DataPoint::getValue() const { return info->getValue(); }

void DataPoint::setValue(const InfoValue &new_value) const {
  info->setValue(new_value);
  switch (type) {
  case M_SP_TB_1:
  case C_SC_TA_1:
  case M_DP_TB_1:
  case C_DC_TA_1:
  case M_ST_TB_1:
  case C_RC_TA_1:
  case M_ME_TD_1:
  case C_SE_TA_1:
  case M_ME_TE_1:
  case C_SE_TB_1:
  case M_ME_TF_1:
  case C_SE_TC_1:
  case M_BO_TB_1:
  case C_BO_TA_1:
  case M_IT_TB_1:
  case M_EP_TD_1:
  case M_EP_TE_1:
  case M_EP_TF_1:
    info->setRecordedAt(DateTime::now(getStation(), false));
    DEBUG_PRINT(
        Debug::Point,
        "Injecting current local timestamp into information for [c104.Type." +
            std::string(TypeID_toString(type)) + "] at IOA " +
            std::to_string(informationObjectAddress));
    break;
  default: {
  }
  }
}

InfoQuality DataPoint::getQuality() const { return info->getQuality(); }

void DataPoint::setQuality(const InfoQuality new_quality) const {
  info->setQuality(new_quality);
  switch (type) {
  case M_SP_TB_1:
  case C_SC_TA_1:
  case M_DP_TB_1:
  case C_DC_TA_1:
  case M_ST_TB_1:
  case C_RC_TA_1:
  case M_ME_TD_1:
  case C_SE_TA_1:
  case M_ME_TE_1:
  case C_SE_TB_1:
  case M_ME_TF_1:
  case C_SE_TC_1:
  case M_BO_TB_1:
  case C_BO_TA_1:
  case M_IT_TB_1:
  case M_EP_TD_1:
  case M_EP_TE_1:
  case M_EP_TF_1:
    info->setRecordedAt(DateTime::now(getStation(), false));
    DEBUG_PRINT(
        Debug::Point,
        "Injecting current local timestamp into information for [c104.Type." +
            std::string(TypeID_toString(type)) + "] at IOA " +
            std::to_string(informationObjectAddress));
    break;
  default: {
  }
  }
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

    // only monitoring points SP,DP,ST,ME,BO + IT
    if (type > M_IT_TB_1 || (type > M_IT_NA_1 && type < M_SP_TB_1)) {
      throw std::invalid_argument(
          "Report interval option is only allowed for "
          "monitoring types (excluding evens), but not for " +
          std::string(TypeID_toString(type)));
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
  if (type == M_IT_TB_1 || type == M_IT_NA_1) {
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

bool DataPoint::transmit(const CS101_CauseOfTransmission cause) {
  DEBUG_PRINT(Debug::Point,
              "transmit_ex] " + std::string(TypeID_toString(type)) +
                  " at IOA " + std::to_string(informationObjectAddress));

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
    return server->transmit(shared_from_this(), cause);
  }

  // as client
  const auto connection = _station->getConnection();
  if (!connection) {
    throw std::invalid_argument("Client connection reference deleted");
  }
  return connection->transmit(shared_from_this(), cause);
}

void DataPoint::detach() { station.reset(); }
