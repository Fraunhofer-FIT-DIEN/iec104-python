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
 * @file PointCommand.cpp
 * @brief create an outgoing from data point in control direction
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "PointCommand.h"
#include "object/DataPoint.h"
#include "object/Information.h"

using namespace Remote::Message;

PointCommand::PointCommand(std::shared_ptr<Object::DataPoint> point,
                           const bool select)
    : OutgoingMessage(point) {
  causeOfTransmission = CS101_COT_ACTIVATION;

  switch (type) {

  case C_SC_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::SingleCmd>(info);
    io = (InformationObject)SingleCommand_create(
        nullptr, informationObjectAddress, i->isOn(), select,
        static_cast<uint8_t>(i->getQualifier()));
  } break;

  case C_SC_TA_1: {
    auto i = std::dynamic_pointer_cast<Object::SingleCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)SingleCommandWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->isOn(), select,
        static_cast<uint8_t>(i->getQualifier()), &time);
  } break;

  case C_DC_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::DoubleCmd>(info);
    io = (InformationObject)DoubleCommand_create(
        nullptr, informationObjectAddress, i->getState(), select,
        static_cast<uint8_t>(i->getQualifier()));
  } break;

  case C_DC_TA_1: {
    auto i = std::dynamic_pointer_cast<Object::DoubleCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)DoubleCommandWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getState(), select,
        static_cast<uint8_t>(i->getQualifier()), &time);
  } break;

  case C_RC_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::StepCmd>(info);
    io = (InformationObject)StepCommand_create(
        nullptr, informationObjectAddress, i->getStep(), select,
        static_cast<uint8_t>(i->getQualifier()));
  } break;

  case C_RC_TA_1: {
    auto i = std::dynamic_pointer_cast<Object::StepCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)StepCommandWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getStep(), select,
        static_cast<uint8_t>(i->getQualifier()), &time);
  } break;

  case C_BO_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::BinaryCmd>(info);
    io = (InformationObject)Bitstring32Command_create(
        nullptr, informationObjectAddress, i->getBlob().get());
  } break;

  case C_BO_TA_1: {
    auto i = std::dynamic_pointer_cast<Object::BinaryCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)Bitstring32CommandWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getBlob().get(), &time);
  } break;

  case C_SE_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::NormalizedCmd>(info);
    io = (InformationObject)SetpointCommandNormalized_create(
        nullptr, informationObjectAddress, i->getTarget().get(), select,
        i->getQualifier().get());
  } break;

  case C_SE_TA_1: {
    auto i = std::dynamic_pointer_cast<Object::NormalizedCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)SetpointCommandNormalizedWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getTarget().get(), select,
        i->getQualifier().get(), &time);
  } break;

  case C_SE_NB_1: {
    auto i = std::dynamic_pointer_cast<Object::ScaledCmd>(info);
    io = (InformationObject)SetpointCommandScaled_create(
        nullptr, informationObjectAddress, i->getTarget().get(), select,
        i->getQualifier().get());
  } break;

  case C_SE_TB_1: {
    auto i = std::dynamic_pointer_cast<Object::ScaledCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)SetpointCommandScaledWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getTarget().get(), select,
        i->getQualifier().get(), &time);
  } break;

    // float Setpoint Command (SHORT)
  case C_SE_NC_1: {
    auto i = std::dynamic_pointer_cast<Object::ShortCmd>(info);
    io = (InformationObject)SetpointCommandShort_create(
        nullptr, informationObjectAddress, i->getTarget(), select,
        i->getQualifier().get());
  } break;

    // float Setpoint Command (SHORT) + Extended Time
  case C_SE_TC_1: {
    auto i = std::dynamic_pointer_cast<Object::ShortCmd>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)SetpointCommandShortWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getTarget(), select,
        i->getQualifier().get(), &time);
  } break;

  default:
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }
}

PointCommand::~PointCommand() {
  if (!io)
    return;

  switch (type) {
  case C_SC_NA_1:
    SingleCommand_destroy((SingleCommand)io);
    break;
  case C_SC_TA_1:
    SingleCommandWithCP56Time2a_destroy((SingleCommandWithCP56Time2a)io);
    break;
  case C_DC_NA_1:
    DoubleCommand_destroy((DoubleCommand)io);
    break;
  case C_DC_TA_1:
    DoubleCommandWithCP56Time2a_destroy((DoubleCommandWithCP56Time2a)io);
    break;
  case C_RC_NA_1:
    StepCommand_destroy((StepCommand)io);
    break;
  case C_RC_TA_1:
    StepCommandWithCP56Time2a_destroy((StepCommandWithCP56Time2a)io);
    break;
  case C_BO_NA_1:
    Bitstring32Command_destroy((Bitstring32Command)io);
    break;
  case C_BO_TA_1:
    Bitstring32CommandWithCP56Time2a_destroy(
        (Bitstring32CommandWithCP56Time2a)io);
    break;
  case C_SE_NA_1:
    SetpointCommandNormalized_destroy((SetpointCommandNormalized)io);
    break;
  case C_SE_TA_1:
    SetpointCommandNormalizedWithCP56Time2a_destroy(
        (SetpointCommandNormalizedWithCP56Time2a)io);
    break;
  case C_SE_NB_1:
    SetpointCommandScaled_destroy((SetpointCommandScaled)io);
    break;
  case C_SE_TB_1:
    SetpointCommandScaledWithCP56Time2a_destroy(
        (SetpointCommandScaledWithCP56Time2a)io);
    break;
  case C_SE_NC_1:
    SetpointCommandShort_destroy((SetpointCommandShort)io);
    break;
  case C_SE_TC_1:
    SetpointCommandShortWithCP56Time2a_destroy(
        (SetpointCommandShortWithCP56Time2a)io);
    break;
  default:
    std::cerr << "[c104.PointCommand] Unsupported type "
              << TypeID_toString(type) << std::endl;
  }
}
