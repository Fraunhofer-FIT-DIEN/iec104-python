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
 * @file IncomingMessage.cpp
 * @brief create a message object from ASDU struct
 * @todo test valid ioa address in series via getIOA()??
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "IncomingMessage.h"

#include "object/Information.h"
#include "remote/Helper.h"
#include <memory>

using namespace Remote::Message;

IncomingMessage::IncomingMessage(CS101_ASDU packet,
                                 CS101_AppLayerParameters app_layer_parameters)
    : IMessageInterface(), asdu(nullptr), parameters(app_layer_parameters),
      position(0), positionReset(true), positionValid(false),
      numberOfObject(0) {
  if (packet) {
    asdu = CS101_ASDU_clone(packet, nullptr);
  }
  if (asdu) {
    extractMetaData();
    first();
  }
  DEBUG_PRINT(Debug::Message, "Created (incoming)");
}

IncomingMessage::~IncomingMessage() {
  if (io) {
    InformationObject_destroy(io);
  }
  if (asdu) {
    CS101_ASDU_destroy(asdu);
  }
  DEBUG_PRINT(Debug::Message, "Removed (incoming)");
}

void IncomingMessage::extractMetaData() {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

    commonAddress = CS101_ASDU_getCA(asdu);
    originatorAddress = CS101_ASDU_getOA(asdu);
    type = CS101_ASDU_getTypeID(asdu);

    causeOfTransmission = CS101_ASDU_getCOT(asdu);

    negative = CS101_ASDU_isNegative(asdu);
    sequence = CS101_ASDU_isSequence(asdu);
    test = CS101_ASDU_isTest(asdu);
  }

  {
    std::lock_guard<Module::GilAwareMutex> const lock_position(position_mutex);
    numberOfObject = (uint_fast8_t)CS101_ASDU_getNumberOfElements(asdu);
  }

  // REJECT CP24Time based messages
  switch (type) {
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
  case M_EP_TC_1:
  case C_TS_NA_1:
  case C_CD_NA_1: {
    throw std::invalid_argument("CP24Time based messages not supported by norm "
                                "IEC60870-5-104 (101 only)!");
  }
  }

  if (type >= C_SC_NA_1 && type < F_DR_TA_1) {
    // REJECT sequence in non-sequence context
    if (sequence) {
      throw std::invalid_argument("IncomingMessage with TypeID " +
                                  std::string(TypeID_toString(type)) +
                                  " must not be marked as sequence.");
    }
    // REJECT multiple objects in non-list context
    if (numberOfObject > 1) {
      throw std::invalid_argument(
          "IncomingMessage with TypeID " + std::string(TypeID_toString(type)) +
          " must not contain more than one information object.");
    }
  }

  // REJECT global common address in non-global context
  if (type < C_IC_NA_1 && type > C_RP_NA_1 &&
      commonAddress == IEC60870_GLOBAL_COMMON_ADDRESS) {
    throw std::invalid_argument(
        "IncomingMessage with TypeID " + std::string(TypeID_toString(type)) +
        " must use a single common address and not the global common address.");
  }

  // REJECT file transfer @todo handle file transfer
  if (type >= F_FR_NA_1) {
    throw std::invalid_argument(
        "lib60870-c does not support file transfer messages.");
  }
}

CS101_ASDU IncomingMessage::getAsdu() const { return asdu; }

unsigned char *IncomingMessage::getRawBytes() const {
  unsigned char *payload = CS101_ASDU_getPayload(asdu);
  unsigned char payloadSize = CS101_ASDU_getPayloadSize(asdu);

  // max total len 255 octets ^= 255 * 8 byte = 255 * 64 bit
  // content len = 4 (control) + number of asdu's
  // start = 68 | len | control1 | control2 | control3 | control 4 ^= 4 octet
  // asdu | ... ^= up to 249 octet
  // start octet = 68

  unsigned char asduHeaderLength =
      2 + parameters->sizeOfCOT + parameters->sizeOfCA;
  unsigned char msgSize = 6 + asduHeaderLength + payloadSize;

  const unsigned char HEADER_OFFSET = 0;
  const unsigned char ASDU_OFFSET = 6;
  const unsigned char PAYLOAD_OFFSET = ASDU_OFFSET + asduHeaderLength;

  auto msg = new unsigned char[msgSize];

  // initialize to zero
  for (unsigned char i = 0; i < msgSize; i++) {
    msg[i] = 0;
  }

  msg[HEADER_OFFSET + 0] = 104;
  msg[HEADER_OFFSET + 1] = 4 + asduHeaderLength +
                           payloadSize; // length following behind length field

  msg[ASDU_OFFSET + 0] = type; // typeId
  msg[ASDU_OFFSET + 1] = numberOfObject;
  if (sequence) {
    msg[ASDU_OFFSET + 1] |= 0x80;
  }
  msg[ASDU_OFFSET + 2] = causeOfTransmission & 0x3f;
  if (test) {
    msg[ASDU_OFFSET + 2] |= 0x80;
  }
  if (negative) {
    msg[ASDU_OFFSET + 2] |= 0x40;
  }
  if (parameters->sizeOfCOT > 1) {
    msg[ASDU_OFFSET + 3] = originatorAddress;
  }
  int caIndex = ASDU_OFFSET + 2 + parameters->sizeOfCOT;
  if (parameters->sizeOfCA == 1) {
    msg[caIndex] = (uint8_t)commonAddress;
  } else {
    msg[caIndex] = (uint8_t)(commonAddress % 0x100);
    msg[caIndex + 1] = (uint8_t)(commonAddress / 0x100);
  }

  for (unsigned char i = 0; i < payloadSize; i++) {
    msg[PAYLOAD_OFFSET + i] = payload[i];
  }

  return msg;
}

std::string IncomingMessage::getRawMessageString() const {
  unsigned char *msg = getRawBytes();
  unsigned char msgSize = 2 + msg[1];

  std::string formatted = rawMessageFormatter(msg, msgSize);
  delete msg;
  return formatted;
}

std::uint_fast8_t IncomingMessage::getNumberOfObject() const {
  return numberOfObject;
}

void IncomingMessage::first() {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);

    positionReset = true;
    position = 0;
    positionValid = (numberOfObject > 0);
  }

  if (positionValid)
    extractInformation();
}

bool IncomingMessage::next() {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);

    if (positionReset) {
      position = 0;
      positionReset = false;
      positionValid = (numberOfObject > 0);
    } else {
      position++;
      positionValid = (position < numberOfObject);
    }
  }

  if (positionValid)
    extractInformation();

  return positionValid;
}

void IncomingMessage::extractInformation() {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  if (io)
    InformationObject_destroy(io);

  io = CS101_ASDU_getElement(asdu, position);
  informationObjectAddress =
      (io == nullptr) ? 0 : InformationObject_getObjectAddress(io);
  info.reset();

  if ((io != nullptr) && positionValid) {
    switch (type) {
      /**
       * MONITORING
       */

    case M_SP_NA_1: {
      info = std::make_shared<Object::SingleInfo>(
          SinglePointInformation_getValue((SinglePointInformation)io),
          static_cast<Quality>(
              SinglePointInformation_getQuality((SinglePointInformation)io)),
          std::nullopt, true);
    } break;

    case M_SP_TB_1: {
      info = std::make_shared<Object::SingleInfo>(
          SinglePointInformation_getValue((SinglePointInformation)io),
          static_cast<Quality>(
              SinglePointInformation_getQuality((SinglePointInformation)io)),
          to_time_point(SinglePointWithCP56Time2a_getTimestamp(
              (SinglePointWithCP56Time2a)io)),
          true);
    } break;

    case M_DP_NA_1: {
      info = std::make_shared<Object::DoubleInfo>(
          DoublePointInformation_getValue((DoublePointInformation)io),
          static_cast<Quality>(
              DoublePointInformation_getQuality((DoublePointInformation)io)),
          std::nullopt, true);
    } break;

    case M_DP_TB_1: {
      info = std::make_shared<Object::DoubleInfo>(
          DoublePointInformation_getValue((DoublePointInformation)io),
          static_cast<Quality>(
              DoublePointInformation_getQuality((DoublePointInformation)io)),
          to_time_point(DoublePointWithCP56Time2a_getTimestamp(
              (DoublePointWithCP56Time2a)io)),
          true);
    } break;

    case M_ST_NA_1: {
      info = std::make_shared<Object::StepInfo>(
          LimitedInt7(
              StepPositionInformation_getValue((StepPositionInformation)io)),
          StepPositionInformation_isTransient((StepPositionInformation)io),
          static_cast<Quality>(
              StepPositionInformation_getQuality((StepPositionInformation)io)),
          std::nullopt, true);
    } break;

    case M_ST_TB_1: {
      info = std::make_shared<Object::StepInfo>(
          LimitedInt7(
              StepPositionInformation_getValue((StepPositionInformation)io)),
          StepPositionInformation_isTransient((StepPositionInformation)io),
          static_cast<Quality>(
              StepPositionInformation_getQuality((StepPositionInformation)io)),
          to_time_point(StepPositionWithCP56Time2a_getTimestamp(
              (StepPositionWithCP56Time2a)io)),
          true);
    } break;

    case M_BO_NA_1: {
      info = std::make_shared<Object::BinaryInfo>(
          Byte32(BitString32_getValue((BitString32)io)),
          static_cast<Quality>(BitString32_getQuality((BitString32)io)),
          std::nullopt, true);
    } break;

    case M_BO_TB_1: {
      info = std::make_shared<Object::BinaryInfo>(
          Byte32(BitString32_getValue((BitString32)io)),
          static_cast<Quality>(BitString32_getQuality((BitString32)io)),
          to_time_point(Bitstring32WithCP56Time2a_getTimestamp(
              (Bitstring32WithCP56Time2a)io)),
          true);
    } break;

    case M_ME_NA_1: {
      info = std::make_shared<Object::NormalizedInfo>(
          NormalizedFloat(
              MeasuredValueNormalized_getValue((MeasuredValueNormalized)io)),
          static_cast<Quality>(
              MeasuredValueNormalized_getQuality((MeasuredValueNormalized)io)),
          std::nullopt, true);
    } break;

    case M_ME_TD_1: {
      info = std::make_shared<Object::NormalizedInfo>(
          NormalizedFloat(
              MeasuredValueNormalized_getValue((MeasuredValueNormalized)io)),
          static_cast<Quality>(
              MeasuredValueNormalized_getQuality((MeasuredValueNormalized)io)),
          to_time_point(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(
              (MeasuredValueNormalizedWithCP56Time2a)io)),
          true);
    } break;

    case M_ME_NB_1: {
      info = std::make_shared<Object::ScaledInfo>(
          LimitedInt16(MeasuredValueScaled_getValue((MeasuredValueScaled)io)),
          static_cast<Quality>(
              MeasuredValueScaled_getQuality((MeasuredValueScaled)io)),
          std::nullopt, true);
    } break;

    case M_ME_TE_1: {
      info = std::make_shared<Object::ScaledInfo>(
          LimitedInt16(MeasuredValueScaled_getValue((MeasuredValueScaled)io)),
          static_cast<Quality>(
              MeasuredValueScaled_getQuality((MeasuredValueScaled)io)),
          to_time_point(MeasuredValueScaledWithCP56Time2a_getTimestamp(
              (MeasuredValueScaledWithCP56Time2a)io)),
          true);
    } break;

    case M_ME_NC_1: {
      info = std::make_shared<Object::ShortInfo>(
          MeasuredValueShort_getValue((MeasuredValueShort)io),
          static_cast<Quality>(
              MeasuredValueShort_getQuality((MeasuredValueShort)io)),
          std::nullopt, true);
    } break;

    case M_ME_TF_1: {
      info = std::make_shared<Object::ShortInfo>(
          MeasuredValueShort_getValue((MeasuredValueShort)io),
          static_cast<Quality>(
              MeasuredValueShort_getQuality((MeasuredValueShort)io)),
          to_time_point(MeasuredValueShortWithCP56Time2a_getTimestamp(
              (MeasuredValueShortWithCP56Time2a)io)),
          true);
    } break;

    case M_IT_NA_1: {
      BinaryCounterReading bcr1 = IntegratedTotals_getBCR((IntegratedTotals)io);
      info = std::make_shared<Object::BinaryCounterInfo>(
          BinaryCounterReading_getValue(bcr1),
          LimitedUInt5(static_cast<uint32_t>(
              BinaryCounterReading_getSequenceNumber(bcr1))),
          BinaryCounterQuality(bcr1->encodedValue[4] & 0b11100000),
          std::nullopt, true);
    } break;

    case M_IT_TB_1: {
      BinaryCounterReading bcr1 = IntegratedTotals_getBCR((IntegratedTotals)io);
      info = std::make_shared<Object::BinaryCounterInfo>(
          BinaryCounterReading_getValue(bcr1),
          LimitedUInt5(static_cast<uint32_t>(
              BinaryCounterReading_getSequenceNumber(bcr1))),
          BinaryCounterQuality(bcr1->encodedValue[4] & 0b11100000),
          to_time_point(IntegratedTotalsWithCP56Time2a_getTimestamp(
              (IntegratedTotalsWithCP56Time2a)io)),
          true);
    } break;

    case M_EP_TD_1: {
      SingleEvent single_event =
          EventOfProtectionEquipmentWithCP56Time2a_getEvent(
              (EventOfProtectionEquipmentWithCP56Time2a)io);
      info = std::make_shared<Object::ProtectionEquipmentEventInfo>(
          static_cast<EventState>(*single_event & 0b00000111),
          LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
              EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
                  (EventOfProtectionEquipmentWithCP56Time2a)io))),
          static_cast<Quality>(*single_event & 0b11111000),
          to_time_point(EventOfProtectionEquipmentWithCP56Time2a_getTimestamp(
              (EventOfProtectionEquipmentWithCP56Time2a)io)),
          true);
    } break;

    case M_EP_TE_1: {
      info = std::make_shared<Object::ProtectionEquipmentStartEventsInfo>(
          StartEvents(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(
                  (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io) &
              0b00111111),
          LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
                  (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io))),
          static_cast<Quality>(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(
                  (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io)),
          to_time_point(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp(
                  (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io)),
          true);
    } break;

    case M_EP_TF_1: {
      info = std::make_shared<Object::ProtectionEquipmentOutputCircuitInfo>(
          OutputCircuits(PackedOutputCircuitInfoWithCP56Time2a_getOCI(
                             (PackedOutputCircuitInfoWithCP56Time2a)io) &
                         0b00001111),
          LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
              PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime(
                  (PackedOutputCircuitInfoWithCP56Time2a)io))),
          static_cast<Quality>(PackedOutputCircuitInfoWithCP56Time2a_getQuality(
              (PackedOutputCircuitInfoWithCP56Time2a)io)),
          to_time_point(PackedOutputCircuitInfoWithCP56Time2a_getTimestamp(
              (PackedOutputCircuitInfoWithCP56Time2a)io)),
          true);
    } break;

    case M_PS_NA_1: {
      StatusAndStatusChangeDetection sscd =
          PackedSinglePointWithSCD_getSCD((PackedSinglePointWithSCD)io);
      info = std::make_shared<Object::StatusWithChangeDetection>(
          FieldSet16(((uint16_t)sscd->encodedValue[0] << 0) +
                     ((uint16_t)sscd->encodedValue[1] << 8)),
          FieldSet16(((uint16_t)sscd->encodedValue[2] << 0) +
                     ((uint16_t)sscd->encodedValue[3] << 8)),
          static_cast<Quality>(PackedSinglePointWithSCD_getQuality(
              (PackedSinglePointWithSCD)io)),
          std::nullopt, true);
    } break;

    case M_ME_ND_1: {
      info = std::make_shared<Object::NormalizedInfo>(
          NormalizedFloat(MeasuredValueNormalizedWithoutQuality_getValue(
              (MeasuredValueNormalizedWithoutQuality)io)),
          Quality::None, std::nullopt, true);
    } break;

      /**
       * CONTROL
       */

    case C_SC_NA_1: {
      info = std::make_shared<Object::SingleCmd>(
          SingleCommand_getState((SingleCommand)io),
          SingleCommand_isSelect((SingleCommand)io),
          static_cast<CS101_QualifierOfCommand>(
              SingleCommand_getQU((SingleCommand)io)),
          std::nullopt, true);
    } break;

    case C_SC_TA_1: {
      info = std::make_shared<Object::SingleCmd>(
          SingleCommand_getState((SingleCommand)io),
          SingleCommand_isSelect((SingleCommand)io),
          static_cast<CS101_QualifierOfCommand>(
              SingleCommand_getQU((SingleCommand)io)),
          to_time_point(SingleCommandWithCP56Time2a_getTimestamp(
              (SingleCommandWithCP56Time2a)io)),
          true);
    } break;

    case C_DC_NA_1: {
      info = std::make_shared<Object::DoubleCmd>(
          static_cast<DoublePointValue>(
              DoubleCommand_getState((DoubleCommand)io)),
          DoubleCommand_isSelect((DoubleCommand)io),
          static_cast<CS101_QualifierOfCommand>(
              DoubleCommand_getQU((DoubleCommand)io)),
          std::nullopt, true);
    } break;

    case C_DC_TA_1: {
      info = std::make_shared<Object::DoubleCmd>(
          static_cast<DoublePointValue>(
              DoubleCommand_getState((DoubleCommand)io)),
          DoubleCommand_isSelect((DoubleCommand)io),
          static_cast<CS101_QualifierOfCommand>(
              DoubleCommand_getQU((DoubleCommand)io)),
          to_time_point(DoubleCommandWithCP56Time2a_getTimestamp(
              (DoubleCommandWithCP56Time2a)io)),
          true);
    } break;

    case C_RC_NA_1: {
      info = std::make_shared<Object::StepCmd>(
          static_cast<StepCommandValue>(StepCommand_getState((StepCommand)io)),
          StepCommand_isSelect((StepCommand)io),
          static_cast<CS101_QualifierOfCommand>(
              StepCommand_getQU((StepCommand)io)),
          std::nullopt, true);
    } break;

    case C_RC_TA_1: {
      info = std::make_shared<Object::StepCmd>(
          static_cast<StepCommandValue>(StepCommand_getState((StepCommand)io)),
          StepCommand_isSelect((StepCommand)io),
          static_cast<CS101_QualifierOfCommand>(
              StepCommand_getQU((StepCommand)io)),
          to_time_point(StepCommandWithCP56Time2a_getTimestamp(
              (StepCommandWithCP56Time2a)io)),
          true);
    } break;

    case C_SE_NA_1: {
      info = std::make_shared<Object::NormalizedCmd>(
          NormalizedFloat(SetpointCommandNormalized_getValue(
              (SetpointCommandNormalized)io)),
          SetpointCommandNormalized_isSelect((SetpointCommandNormalized)io),
          LimitedUInt7(static_cast<uint32_t>(
              SetpointCommandNormalized_getQL((SetpointCommandNormalized)io))),
          std::nullopt, true);
    } break;

    case C_SE_TA_1: {
      info = std::make_shared<Object::NormalizedCmd>(
          NormalizedFloat(SetpointCommandNormalized_getValue(
              (SetpointCommandNormalized)io)),
          SetpointCommandNormalized_isSelect((SetpointCommandNormalized)io),
          LimitedUInt7(static_cast<uint32_t>(
              SetpointCommandNormalized_getQL((SetpointCommandNormalized)io))),
          to_time_point(SetpointCommandNormalizedWithCP56Time2a_getTimestamp(
              (SetpointCommandNormalizedWithCP56Time2a)io)),
          true);
    } break;

    case C_SE_NB_1: {
      info = std::make_shared<Object::ScaledCmd>(
          LimitedInt16(
              SetpointCommandScaled_getValue((SetpointCommandScaled)io)),
          SetpointCommandScaled_isSelect((SetpointCommandScaled)io),
          LimitedUInt7(static_cast<uint32_t>(
              SetpointCommandScaled_getQL((SetpointCommandScaled)io))),
          std::nullopt, true);
    } break;

    case C_SE_TB_1: {
      info = std::make_shared<Object::ScaledCmd>(
          LimitedInt16(
              SetpointCommandScaled_getValue((SetpointCommandScaled)io)),
          SetpointCommandScaled_isSelect((SetpointCommandScaled)io),
          LimitedUInt7(static_cast<uint32_t>(
              SetpointCommandScaled_getQL((SetpointCommandScaled)io))),
          to_time_point(SetpointCommandScaledWithCP56Time2a_getTimestamp(
              (SetpointCommandScaledWithCP56Time2a)io)),
          true);
    } break;

    case C_SE_NC_1: {
      info = std::make_shared<Object::ShortCmd>(
          SetpointCommandShort_getValue((SetpointCommandShort)io),
          SetpointCommandShort_isSelect((SetpointCommandShort)io),
          LimitedUInt7(static_cast<uint32_t>(
              SetpointCommandShort_getQL((SetpointCommandShort)io))),
          std::nullopt, true);
    } break;

    case C_SE_TC_1: {
      info = std::make_shared<Object::ShortCmd>(
          SetpointCommandShort_getValue((SetpointCommandShort)io),
          SetpointCommandShort_isSelect((SetpointCommandShort)io),
          LimitedUInt7(static_cast<uint32_t>(
              SetpointCommandShort_getQL((SetpointCommandShort)io))),
          to_time_point(SetpointCommandShortWithCP56Time2a_getTimestamp(
              (SetpointCommandShortWithCP56Time2a)io)),
          true);
    } break;

    case C_BO_NA_1: {
      info = std::make_shared<Object::BinaryCmd>(
          Byte32(Bitstring32Command_getValue((Bitstring32Command)io)),
          std::nullopt, true);
    } break;

    case C_BO_TA_1: {
      info = std::make_shared<Object::BinaryCmd>(
          Byte32(Bitstring32Command_getValue((Bitstring32Command)io)),
          to_time_point(Bitstring32CommandWithCP56Time2a_getTimestamp(
              (Bitstring32CommandWithCP56Time2a)io)),
          true);
    } break;
    case C_CS_NA_1: {
      info = std::make_shared<Object::Command>(
          to_time_point(ClockSynchronizationCommand_getTime(
              (ClockSynchronizationCommand)io)),
          true);
    } break;

    case C_IC_NA_1:
    case C_CI_NA_1:
    case C_RD_NA_1:
    case C_TS_NA_1:
      // allow get valid message to pass and extract informationObjectAddress
      // even though no further info is extracted
      break;

      /**
       * Unhandled message
       */

    default:
      std::cerr << "[c104.IncomingMessage.extract] Unsupported type "
                << TypeID_toString(type) << std::endl;
    }
  }
}

bool IncomingMessage::isValidCauseOfTransmission() const {
  bool result = true;
  switch (type) {
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
  case M_EP_TC_1:
  case C_TS_NA_1:
  case C_CD_NA_1: {
    throw std::invalid_argument(
        "[104.IncomingMessage] type not supported by norm IEC60870-5-104!");
  } break;
  case M_SP_NA_1:
  case M_DP_NA_1:
  case M_ST_NA_1: {
    if (causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_RETURN_INFO_REMOTE &&
        causeOfTransmission != CS101_COT_RETURN_INFO_LOCAL &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_BO_NA_1:
  case M_PS_NA_1: {
    if (causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_ME_NA_1:
  case M_ME_NB_1:
  case M_ME_NC_1:
  case M_ME_ND_1: {
    if (causeOfTransmission != CS101_COT_PERIODIC &&
        causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_IT_NA_1:
  case M_IT_TB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        (causeOfTransmission < CS101_COT_REQUESTED_BY_GENERAL_COUNTER &&
         causeOfTransmission > CS101_COT_REQUESTED_BY_GROUP_4_COUNTER)) {
      result = false;
    }
  } break;
  case M_SP_TB_1:
  case M_DP_TB_1:
  case M_ST_TB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_RETURN_INFO_REMOTE &&
        causeOfTransmission != CS101_COT_RETURN_INFO_LOCAL) {
      result = false;
    }
  } break;
  case M_BO_TB_1:
  case M_ME_TD_1:
  case M_ME_TE_1:
  case M_ME_TF_1:
  case F_DR_TA_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST) {
      result = false;
    }
  } break;
  case M_EP_TD_1:
  case M_EP_TE_1:
  case M_EP_TF_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS) {
      result = false;
    }
  } break;
  case C_SC_NA_1:
  case C_SC_TA_1:
  case C_DC_NA_1:
  case C_DC_TA_1:
  case C_RC_NA_1:
  case C_RC_TA_1:
  case C_SE_NA_1:
  case C_SE_TA_1:
  case C_SE_NB_1:
  case C_SE_TB_1:
  case C_SE_NC_1:
  case C_SE_TC_1:
  case C_IC_NA_1: {
    if ((causeOfTransmission < CS101_COT_ACTIVATION &&
         causeOfTransmission > CS101_COT_ACTIVATION_TERMINATION) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_BO_NA_1:
  case C_BO_TA_1:
  case C_CI_NA_1: {
    if ((causeOfTransmission != CS101_COT_ACTIVATION &&
         causeOfTransmission != CS101_COT_ACTIVATION_CON &&
         causeOfTransmission != CS101_COT_ACTIVATION_TERMINATION) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case M_EI_NA_1: {
    if (causeOfTransmission != CS101_COT_INITIALIZED) {
      result = false;
    }
  } break;
  case C_RD_NA_1: {
    if (causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_CS_NA_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        // causeOfTransmission != CS101_COT_SPONTANEOUS && only cs 101, not
        // allowed in 104
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_RP_NA_1:
  case C_TS_TA_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case P_ME_NA_1:
  case P_ME_NB_1:
  case P_ME_NC_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION &&
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case P_AC_NA_1: {
    if ((causeOfTransmission < CS101_COT_ACTIVATION &&
         causeOfTransmission > CS101_COT_DEACTIVATION_CON) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_FR_NA_1:
  case F_SR_NA_1:
  case F_LS_NA_1:
  case F_AF_NA_1:
  case F_SG_NA_1: {
    if (causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_SC_NA_1: {
    if (causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_SC_NB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID &&
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  }

  return result;
}

bool IncomingMessage::requireConfirmation() const {
  return (causeOfTransmission == CS101_COT_ACTIVATION ||
          causeOfTransmission == CS101_COT_DEACTIVATION);
}

bool IncomingMessage::isSelectCommand() const {
  auto cmd = std::dynamic_pointer_cast<Object::Command>(info);
  return cmd && cmd->isSelectable() && cmd->isSelect();
}

std::string IncomingMessage::toString() const {
  std::ostringstream oss;
  oss << "<c104.IncomingMessage common_address="
      << std::to_string(commonAddress)
      << ", io_address=" << std::to_string(informationObjectAddress)
      << ", type=" << TypeID_toString(type) << ", info=" << info->name()
      << ", cot=" << CS101_CauseOfTransmission_toString(causeOfTransmission)
      << ", test=" << bool_toString(test)
      << ", negative=" << bool_toString(negative)
      << ", sequence=" << bool_toString(sequence) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
};
