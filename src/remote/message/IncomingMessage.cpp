/**
 * Copyright 2020-2026 Fraunhofer Institute for Applied Information Technology
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

#define APDU_OFFSET 0
#define ASDU_OFFSET 6

using namespace Remote::Message;

IncomingMessage::IncomingMessage(CS101_ASDU packet,
                                 CS101_AppLayerParameters app_layer_parameters)
    : IMessageInterface(), asdu(nullptr), position(0), positionReset(true),
      positionValid(false), numberOfObjects(0) {
  if (packet && !app_layer_parameters) {
    throw std::invalid_argument(
        "IncomingMessage requires application layer parameters");
  }
  if (packet) {
    // keep a private copy of the parameters: the message is handed to Python
    // and may outlive the Client/Server/Connection that owns the originals
    parameters = *app_layer_parameters;
    asdu = Remote::cloneAsdu(packet, &parameters);
  }
  if (asdu) {
    // a throwing constructor never runs the destructor: release the clone and
    // a partially extracted information object before propagating the error
    try {
      extractMetaData();
      first();
    } catch (...) {
      if (io) {
        InformationObject_destroy(io);
        io = nullptr;
      }
      CS101_ASDU_destroy(asdu);
      asdu = nullptr;
      throw;
    }
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
    numberOfObjects = (uint_fast8_t)CS101_ASDU_getNumberOfElements(asdu);
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
  case M_EP_TC_1: {
    throw std::invalid_argument("CP24Time based messages not supported by norm "
                                "IEC60870-5-104 (101 only)!");
  }
  case C_TS_NA_1:
  case C_CD_NA_1: {
    throw std::invalid_argument("Message not supported by norm "
                                "IEC60870-5-104 (101 only)!");
  }
  }

  // REJECT empty messages: every IEC60870-5-104 ASDU carries at least one
  // information object; lib60870 does not validate the payload for VSQ=0, and
  // a zero count would leave io/info null for every consumer
  if (numberOfObjects == 0) {
    throw std::invalid_argument(
        "IncomingMessage with TypeID " + std::string(TypeID_toString(type)) +
        " must contain at least one information object.");
  }

  if (type >= C_SC_NA_1 && type < F_DR_TA_1) {
    // REJECT sequence in non-sequence context
    if (sequence) {
      throw std::invalid_argument("IncomingMessage with TypeID " +
                                  std::string(TypeID_toString(type)) +
                                  " must not be marked as sequence.");
    }
    // REJECT multiple objects in non-list context
    if (numberOfObjects > 1) {
      throw std::invalid_argument(
          "IncomingMessage with TypeID " + std::string(TypeID_toString(type)) +
          " must not contain more than one information object.");
    }
  }

  // REJECT global common address in non-global context
  if ((type < C_IC_NA_1 || type > C_RP_NA_1) &&
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
  // max APDU len 255 octets ^= 255 * 8 byte
  // start octet = 68
  // start = 68 | len | control1 | control2 | control3 | control 4 ^= 4 octet
  // ASDU (max len 249 octet)
  //  - HEADER
  //  - PAYLOAD

  const auto asduHeaderLength =
      static_cast<size_t>(2 + parameters.sizeOfCOT + parameters.sizeOfCA);
  const size_t PAYLOAD_OFFSET = ASDU_OFFSET + asduHeaderLength;
  // clamp payloadSize to max allowed length
  const auto clampedPayloadSize = static_cast<size_t>(std::min(
      CS101_ASDU_getPayloadSize(asdu), 255 - static_cast<int>(PAYLOAD_OFFSET)));
  const size_t msgSize = PAYLOAD_OFFSET + clampedPayloadSize;

  unsigned char *payload = CS101_ASDU_getPayload(asdu);
  const auto msg = new unsigned char[msgSize];

  // initialize to zero
  for (size_t i = 0; i < msgSize; i++) {
    msg[i] = 0;
  }

  msg[APDU_OFFSET + 0] = 104;         // start octet = 68
  msg[APDU_OFFSET + 1] = msgSize - 2; // length octet (without start octet)
  // msg[APDU_OFFSET + 2] = 0; // control 1
  // msg[APDU_OFFSET + 3] = 0; // control 2
  // msg[APDU_OFFSET + 4] = 0; // control 3
  // msg[APDU_OFFSET + 5] = 0; // control 4

  msg[ASDU_OFFSET + 0] = type; // typeId
  msg[ASDU_OFFSET + 1] = numberOfObjects;
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
  if (parameters.sizeOfCOT > 1) {
    msg[ASDU_OFFSET + 3] = originatorAddress;
  }
  int caIndex = ASDU_OFFSET + 2 + parameters.sizeOfCOT;
  if (parameters.sizeOfCA == 1) {
    msg[caIndex] = static_cast<uint8_t>(commonAddress);
  } else {
    msg[caIndex] = static_cast<uint8_t>(commonAddress % 0x100);
    msg[caIndex + 1] = static_cast<uint8_t>(commonAddress / 0x100);
  }

  for (size_t i = 0; i < clampedPayloadSize; i++) {
    msg[PAYLOAD_OFFSET + i] = payload[i];
  }

  return msg;
}

std::string IncomingMessage::getRawMessageString() const {
  const unsigned char *msg = getRawBytes();
  const int msgSize = 2 + msg[1];

  std::string formatted = rawMessageFormatter(msg, msgSize);
  delete[] msg;
  return formatted;
}

std::uint_fast8_t IncomingMessage::getNumberOfObjects() const {
  return numberOfObjects;
}

void IncomingMessage::first() {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);

    positionReset = true;
    position = 0;
    positionValid = (numberOfObjects > 0);
  }

  if (positionValid)
    extractInformation();
}

std::shared_ptr<IncomingMessage> IncomingMessage::snapshot() const {
  // the constructor copies the parameter block, the const_cast is never
  // written through
  auto copy = create(asdu, const_cast<CS101_AppLayerParameters>(&parameters));
  std::uint_fast8_t pos = 0;
  bool reset = true;
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);
    pos = position;
    reset = positionReset;
  }
  if (!reset) {
    copy->seek(pos);
  }
  return copy;
}

void IncomingMessage::seek(const std::uint_fast8_t pos) {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);
    positionReset = false;
    position = pos;
    positionValid = (pos < numberOfObjects);
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
      positionValid = (numberOfObjects > 0);
    } else {
      ++position;
      positionValid = (position < numberOfObjects);
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
          SinglePointInformation_getValue(
              reinterpret_cast<SinglePointInformation>(io)),
          static_cast<Quality>(SinglePointInformation_getQuality(
              reinterpret_cast<SinglePointInformation>(io))),
          std::nullopt, true);
    } break;

    case M_SP_TB_1: {
      info = std::make_shared<Object::SingleInfo>(
          SinglePointInformation_getValue(
              reinterpret_cast<SinglePointInformation>(io)),
          static_cast<Quality>(SinglePointInformation_getQuality(
              reinterpret_cast<SinglePointInformation>(io))),
          to_time_point(SinglePointWithCP56Time2a_getTimestamp(
              reinterpret_cast<SinglePointWithCP56Time2a>(io))),
          true);
    } break;

    case M_DP_NA_1: {
      info = std::make_shared<Object::DoubleInfo>(
          DoublePointInformation_getValue(
              reinterpret_cast<DoublePointInformation>(io)),
          static_cast<Quality>(DoublePointInformation_getQuality(
              reinterpret_cast<DoublePointInformation>(io))),
          std::nullopt, true);
    } break;

    case M_DP_TB_1: {
      info = std::make_shared<Object::DoubleInfo>(
          DoublePointInformation_getValue(
              reinterpret_cast<DoublePointInformation>(io)),
          static_cast<Quality>(DoublePointInformation_getQuality(
              reinterpret_cast<DoublePointInformation>(io))),
          to_time_point(DoublePointWithCP56Time2a_getTimestamp(
              reinterpret_cast<DoublePointWithCP56Time2a>(io))),
          true);
    } break;

    case M_ST_NA_1: {
      info = std::make_shared<Object::StepInfo>(
          LimitedInt7(StepPositionInformation_getValue(
              reinterpret_cast<StepPositionInformation>(io))),
          StepPositionInformation_isTransient(
              reinterpret_cast<StepPositionInformation>(io)),
          static_cast<Quality>(StepPositionInformation_getQuality(
              reinterpret_cast<StepPositionInformation>(io))),
          std::nullopt, true);
    } break;

    case M_ST_TB_1: {
      info = std::make_shared<Object::StepInfo>(
          LimitedInt7(StepPositionInformation_getValue(
              reinterpret_cast<StepPositionInformation>(io))),
          StepPositionInformation_isTransient(
              reinterpret_cast<StepPositionInformation>(io)),
          static_cast<Quality>(StepPositionInformation_getQuality(
              reinterpret_cast<StepPositionInformation>(io))),
          to_time_point(StepPositionWithCP56Time2a_getTimestamp(
              reinterpret_cast<StepPositionWithCP56Time2a>(io))),
          true);
    } break;

    case M_BO_NA_1: {
      info = std::make_shared<Object::BinaryInfo>(
          Byte32(BitString32_getValue(reinterpret_cast<BitString32>(io))),
          static_cast<Quality>(
              BitString32_getQuality(reinterpret_cast<BitString32>(io))),
          std::nullopt, true);
    } break;

    case M_BO_TB_1: {
      info = std::make_shared<Object::BinaryInfo>(
          Byte32(BitString32_getValue(reinterpret_cast<BitString32>(io))),
          static_cast<Quality>(
              BitString32_getQuality(reinterpret_cast<BitString32>(io))),
          to_time_point(Bitstring32WithCP56Time2a_getTimestamp(
              reinterpret_cast<Bitstring32WithCP56Time2a>(io))),
          true);
    } break;

    case M_ME_NA_1: {
      info = std::make_shared<Object::NormalizedInfo>(
          NormalizedFloat(MeasuredValueNormalized_getValue(
              reinterpret_cast<MeasuredValueNormalized>(io))),
          static_cast<Quality>(MeasuredValueNormalized_getQuality(
              reinterpret_cast<MeasuredValueNormalized>(io))),
          std::nullopt, true);
    } break;

    case M_ME_TD_1: {
      info = std::make_shared<Object::NormalizedInfo>(
          NormalizedFloat(MeasuredValueNormalized_getValue(
              reinterpret_cast<MeasuredValueNormalized>(io))),
          static_cast<Quality>(MeasuredValueNormalized_getQuality(
              reinterpret_cast<MeasuredValueNormalized>(io))),
          to_time_point(MeasuredValueNormalizedWithCP56Time2a_getTimestamp(
              reinterpret_cast<MeasuredValueNormalizedWithCP56Time2a>(io))),
          true);
    } break;

    case M_ME_NB_1: {
      info = std::make_shared<Object::ScaledInfo>(
          LimitedInt16(MeasuredValueScaled_getValue(
              reinterpret_cast<MeasuredValueScaled>(io))),
          static_cast<Quality>(MeasuredValueScaled_getQuality(
              reinterpret_cast<MeasuredValueScaled>(io))),
          std::nullopt, true);
    } break;

    case M_ME_TE_1: {
      info = std::make_shared<Object::ScaledInfo>(
          LimitedInt16(MeasuredValueScaled_getValue(
              reinterpret_cast<MeasuredValueScaled>(io))),
          static_cast<Quality>(MeasuredValueScaled_getQuality(
              reinterpret_cast<MeasuredValueScaled>(io))),
          to_time_point(MeasuredValueScaledWithCP56Time2a_getTimestamp(
              reinterpret_cast<MeasuredValueScaledWithCP56Time2a>(io))),
          true);
    } break;

    case M_ME_NC_1: {
      info = std::make_shared<Object::ShortInfo>(
          MeasuredValueShort_getValue(reinterpret_cast<MeasuredValueShort>(io)),
          static_cast<Quality>(MeasuredValueShort_getQuality(
              reinterpret_cast<MeasuredValueShort>(io))),
          std::nullopt, true);
    } break;

    case M_ME_TF_1: {
      info = std::make_shared<Object::ShortInfo>(
          MeasuredValueShort_getValue(reinterpret_cast<MeasuredValueShort>(io)),
          static_cast<Quality>(MeasuredValueShort_getQuality(
              reinterpret_cast<MeasuredValueShort>(io))),
          to_time_point(MeasuredValueShortWithCP56Time2a_getTimestamp(
              reinterpret_cast<MeasuredValueShortWithCP56Time2a>(io))),
          true);
    } break;

    case M_IT_NA_1: {
      BinaryCounterReading bcr1 =
          IntegratedTotals_getBCR(reinterpret_cast<IntegratedTotals>(io));
      info = std::make_shared<Object::BinaryCounterInfo>(
          BinaryCounterReading_getValue(bcr1),
          LimitedUInt5(static_cast<uint32_t>(
              BinaryCounterReading_getSequenceNumber(bcr1))),
          static_cast<BinaryCounterQuality>(bcr1->encodedValue[4] & 0b11100000),
          std::nullopt, true);
    } break;

    case M_IT_TB_1: {
      BinaryCounterReading bcr1 =
          IntegratedTotals_getBCR(reinterpret_cast<IntegratedTotals>(io));
      info = std::make_shared<Object::BinaryCounterInfo>(
          BinaryCounterReading_getValue(bcr1),
          LimitedUInt5(static_cast<uint32_t>(
              BinaryCounterReading_getSequenceNumber(bcr1))),
          static_cast<BinaryCounterQuality>(bcr1->encodedValue[4] & 0b11100000),
          to_time_point(IntegratedTotalsWithCP56Time2a_getTimestamp(
              reinterpret_cast<IntegratedTotalsWithCP56Time2a>(io))),
          true);
    } break;

    case M_EP_TD_1: {
      SingleEvent single_event =
          EventOfProtectionEquipmentWithCP56Time2a_getEvent(
              reinterpret_cast<EventOfProtectionEquipmentWithCP56Time2a>(io));
      info = std::make_shared<Object::ProtectionEquipmentEventInfo>(
          static_cast<EventState>(*single_event & 0b00000111),
          LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
              EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
                  reinterpret_cast<EventOfProtectionEquipmentWithCP56Time2a>(
                      io)))),
          static_cast<Quality>(*single_event & 0b11111000),
          to_time_point(EventOfProtectionEquipmentWithCP56Time2a_getTimestamp(
              reinterpret_cast<EventOfProtectionEquipmentWithCP56Time2a>(io))),
          true);
    } break;

    case M_EP_TE_1: {
      info = std::make_shared<Object::ProtectionEquipmentStartEventsInfo>(
          static_cast<StartEvents>(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(
                  reinterpret_cast<
                      PackedStartEventsOfProtectionEquipmentWithCP56Time2a>(
                      io)) &
              0b00111111),
          LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
                  reinterpret_cast<
                      PackedStartEventsOfProtectionEquipmentWithCP56Time2a>(
                      io)))),
          static_cast<Quality>(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(
                  reinterpret_cast<
                      PackedStartEventsOfProtectionEquipmentWithCP56Time2a>(
                      io))),
          to_time_point(
              PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp(
                  reinterpret_cast<
                      PackedStartEventsOfProtectionEquipmentWithCP56Time2a>(
                      io))),
          true);
    } break;

    case M_EP_TF_1: {
      info = std::make_shared<Object::ProtectionEquipmentOutputCircuitInfo>(
          static_cast<OutputCircuits>(
              PackedOutputCircuitInfoWithCP56Time2a_getOCI(
                  reinterpret_cast<PackedOutputCircuitInfoWithCP56Time2a>(io)) &
              0b00001111),
          LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
              PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime(
                  reinterpret_cast<PackedOutputCircuitInfoWithCP56Time2a>(
                      io)))),
          static_cast<Quality>(PackedOutputCircuitInfoWithCP56Time2a_getQuality(
              reinterpret_cast<PackedOutputCircuitInfoWithCP56Time2a>(io))),
          to_time_point(PackedOutputCircuitInfoWithCP56Time2a_getTimestamp(
              reinterpret_cast<PackedOutputCircuitInfoWithCP56Time2a>(io))),
          true);
    } break;

    case M_PS_NA_1: {
      StatusAndStatusChangeDetection sscd = PackedSinglePointWithSCD_getSCD(
          reinterpret_cast<PackedSinglePointWithSCD>(io));
      info = std::make_shared<Object::StatusWithChangeDetection>(
          static_cast<FieldSet16>(
              (static_cast<uint16_t>(sscd->encodedValue[0]) << 0) +
              (static_cast<uint16_t>(sscd->encodedValue[1]) << 8)),
          static_cast<FieldSet16>(
              (static_cast<uint16_t>(sscd->encodedValue[2]) << 0) +
              (static_cast<uint16_t>(sscd->encodedValue[3]) << 8)),
          static_cast<Quality>(PackedSinglePointWithSCD_getQuality(
              reinterpret_cast<PackedSinglePointWithSCD>(io))),
          std::nullopt, true);
    } break;

    case M_ME_ND_1: {
      info = std::make_shared<Object::NormalizedInfo>(
          NormalizedFloat(MeasuredValueNormalizedWithoutQuality_getValue(
              reinterpret_cast<MeasuredValueNormalizedWithoutQuality>(io))),
          Quality::None, std::nullopt, true);
    } break;

      /**
       * CONTROL
       */

    case C_SC_NA_1: {
      info = std::make_shared<Object::SingleCmd>(
          SingleCommand_getState(reinterpret_cast<SingleCommand>(io)),
          SingleCommand_isSelect(reinterpret_cast<SingleCommand>(io)),
          static_cast<CS101_QualifierOfCommand>(
              SingleCommand_getQU(reinterpret_cast<SingleCommand>(io))),
          std::nullopt, true);
    } break;

    case C_SC_TA_1: {
      info = std::make_shared<Object::SingleCmd>(
          SingleCommand_getState(reinterpret_cast<SingleCommand>(io)),
          SingleCommand_isSelect(reinterpret_cast<SingleCommand>(io)),
          static_cast<CS101_QualifierOfCommand>(
              SingleCommand_getQU(reinterpret_cast<SingleCommand>(io))),
          to_time_point(SingleCommandWithCP56Time2a_getTimestamp(
              reinterpret_cast<SingleCommandWithCP56Time2a>(io))),
          true);
    } break;

    case C_DC_NA_1: {
      info = std::make_shared<Object::DoubleCmd>(
          static_cast<DoublePointValue>(
              DoubleCommand_getState(reinterpret_cast<DoubleCommand>(io))),
          DoubleCommand_isSelect(reinterpret_cast<DoubleCommand>(io)),
          static_cast<CS101_QualifierOfCommand>(
              DoubleCommand_getQU(reinterpret_cast<DoubleCommand>(io))),
          std::nullopt, true);
    } break;

    case C_DC_TA_1: {
      info = std::make_shared<Object::DoubleCmd>(
          static_cast<DoublePointValue>(
              DoubleCommand_getState(reinterpret_cast<DoubleCommand>(io))),
          DoubleCommand_isSelect(reinterpret_cast<DoubleCommand>(io)),
          static_cast<CS101_QualifierOfCommand>(
              DoubleCommand_getQU(reinterpret_cast<DoubleCommand>(io))),
          to_time_point(DoubleCommandWithCP56Time2a_getTimestamp(
              reinterpret_cast<DoubleCommandWithCP56Time2a>(io))),
          true);
    } break;

    case C_RC_NA_1: {
      info = std::make_shared<Object::StepCmd>(
          static_cast<StepCommandValue>(
              StepCommand_getState(reinterpret_cast<StepCommand>(io))),
          StepCommand_isSelect(reinterpret_cast<StepCommand>(io)),
          static_cast<CS101_QualifierOfCommand>(
              StepCommand_getQU(reinterpret_cast<StepCommand>(io))),
          std::nullopt, true);
    } break;

    case C_RC_TA_1: {
      info = std::make_shared<Object::StepCmd>(
          static_cast<StepCommandValue>(
              StepCommand_getState(reinterpret_cast<StepCommand>(io))),
          StepCommand_isSelect(reinterpret_cast<StepCommand>(io)),
          static_cast<CS101_QualifierOfCommand>(
              StepCommand_getQU(reinterpret_cast<StepCommand>(io))),
          to_time_point(StepCommandWithCP56Time2a_getTimestamp(
              reinterpret_cast<StepCommandWithCP56Time2a>(io))),
          true);
    } break;

    case C_SE_NA_1: {
      info = std::make_shared<Object::NormalizedCmd>(
          NormalizedFloat(SetpointCommandNormalized_getValue(
              reinterpret_cast<SetpointCommandNormalized>(io))),
          SetpointCommandNormalized_isSelect(
              reinterpret_cast<SetpointCommandNormalized>(io)),
          LimitedUInt7(static_cast<uint32_t>(SetpointCommandNormalized_getQL(
              reinterpret_cast<SetpointCommandNormalized>(io)))),
          std::nullopt, true);
    } break;

    case C_SE_TA_1: {
      info = std::make_shared<Object::NormalizedCmd>(
          NormalizedFloat(SetpointCommandNormalized_getValue(
              reinterpret_cast<SetpointCommandNormalized>(io))),
          SetpointCommandNormalized_isSelect(
              reinterpret_cast<SetpointCommandNormalized>(io)),
          LimitedUInt7(static_cast<uint32_t>(SetpointCommandNormalized_getQL(
              reinterpret_cast<SetpointCommandNormalized>(io)))),
          to_time_point(SetpointCommandNormalizedWithCP56Time2a_getTimestamp(
              reinterpret_cast<SetpointCommandNormalizedWithCP56Time2a>(io))),
          true);
    } break;

    case C_SE_NB_1: {
      info = std::make_shared<Object::ScaledCmd>(
          LimitedInt16(SetpointCommandScaled_getValue(
              reinterpret_cast<SetpointCommandScaled>(io))),
          SetpointCommandScaled_isSelect(
              reinterpret_cast<SetpointCommandScaled>(io)),
          LimitedUInt7(static_cast<uint32_t>(SetpointCommandScaled_getQL(
              reinterpret_cast<SetpointCommandScaled>(io)))),
          std::nullopt, true);
    } break;

    case C_SE_TB_1: {
      info = std::make_shared<Object::ScaledCmd>(
          LimitedInt16(SetpointCommandScaled_getValue(
              reinterpret_cast<SetpointCommandScaled>(io))),
          SetpointCommandScaled_isSelect(
              reinterpret_cast<SetpointCommandScaled>(io)),
          LimitedUInt7(static_cast<uint32_t>(SetpointCommandScaled_getQL(
              reinterpret_cast<SetpointCommandScaled>(io)))),
          to_time_point(SetpointCommandScaledWithCP56Time2a_getTimestamp(
              reinterpret_cast<SetpointCommandScaledWithCP56Time2a>(io))),
          true);
    } break;

    case C_SE_NC_1: {
      info = std::make_shared<Object::ShortCmd>(
          SetpointCommandShort_getValue(
              reinterpret_cast<SetpointCommandShort>(io)),
          SetpointCommandShort_isSelect(
              reinterpret_cast<SetpointCommandShort>(io)),
          LimitedUInt7(static_cast<uint32_t>(SetpointCommandShort_getQL(
              reinterpret_cast<SetpointCommandShort>(io)))),
          std::nullopt, true);
    } break;

    case C_SE_TC_1: {
      info = std::make_shared<Object::ShortCmd>(
          SetpointCommandShort_getValue(
              reinterpret_cast<SetpointCommandShort>(io)),
          SetpointCommandShort_isSelect(
              reinterpret_cast<SetpointCommandShort>(io)),
          LimitedUInt7(static_cast<uint32_t>(SetpointCommandShort_getQL(
              reinterpret_cast<SetpointCommandShort>(io)))),
          to_time_point(SetpointCommandShortWithCP56Time2a_getTimestamp(
              reinterpret_cast<SetpointCommandShortWithCP56Time2a>(io))),
          true);
    } break;

    case C_BO_NA_1: {
      info = std::make_shared<Object::BinaryCmd>(
          Byte32(Bitstring32Command_getValue(
              reinterpret_cast<Bitstring32Command>(io))),
          std::nullopt, true);
    } break;

    case C_BO_TA_1: {
      info = std::make_shared<Object::BinaryCmd>(
          Byte32(Bitstring32Command_getValue(
              reinterpret_cast<Bitstring32Command>(io))),
          to_time_point(Bitstring32CommandWithCP56Time2a_getTimestamp(
              reinterpret_cast<Bitstring32CommandWithCP56Time2a>(io))),
          true);
    } break;
    case C_CS_NA_1: {
      info = std::make_shared<Object::Command>(
          to_time_point(ClockSynchronizationCommand_getTime(
              reinterpret_cast<ClockSynchronizationCommand>(io))),
          true);
    } break;

    case M_EI_NA_1:
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
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION ||
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_BO_NA_1:
  case M_PS_NA_1: {
    if (causeOfTransmission != CS101_COT_BACKGROUND_SCAN &&
        causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION ||
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
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION ||
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16)) {
      result = false;
    }
  } break;
  case M_IT_NA_1:
  case M_IT_TB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        (causeOfTransmission < CS101_COT_REQUESTED_BY_GENERAL_COUNTER ||
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
    if ((causeOfTransmission < CS101_COT_ACTIVATION ||
         causeOfTransmission > CS101_COT_ACTIVATION_TERMINATION) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
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
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
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
    // Read commands are sent with COT=REQUEST (5) per IEC 60870-5.
    // The server responds with the value (COT=REQUEST) and ACT_CON
    // (COT=ACTIVATION_CON=7).
    // Error responses use the range [UNKNOWN_TYPE_ID..UNKNOWN_IOA] = [44..47].
    if (causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_CS_NA_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        // causeOfTransmission != CS101_COT_SPONTANEOUS && only cs 101, not
        // allowed in 104
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case C_RP_NA_1:
  case C_TS_TA_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case P_ME_NA_1:
  case P_ME_NB_1:
  case P_ME_NC_1: {
    if (causeOfTransmission != CS101_COT_ACTIVATION &&
        causeOfTransmission != CS101_COT_ACTIVATION_CON &&
        (causeOfTransmission < CS101_COT_INTERROGATED_BY_STATION ||
         causeOfTransmission > CS101_COT_INTERROGATED_BY_GROUP_16) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case P_AC_NA_1: {
    if ((causeOfTransmission < CS101_COT_ACTIVATION ||
         causeOfTransmission > CS101_COT_DEACTIVATION_CON) &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
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
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_SC_NA_1: {
    if (causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
         causeOfTransmission > CS101_COT_UNKNOWN_IOA)) {
      result = false;
    }
  } break;
  case F_SC_NB_1: {
    if (causeOfTransmission != CS101_COT_SPONTANEOUS &&
        causeOfTransmission != CS101_COT_REQUEST &&
        causeOfTransmission != CS101_COT_FILE_TRANSFER &&
        (causeOfTransmission < CS101_COT_UNKNOWN_TYPE_ID ||
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
      << ", type=" << TypeID_toString(type)
      << ", info=" << (info ? info->name() : "none")
      << ", cot=" << CS101_CauseOfTransmission_toString(causeOfTransmission)
      << ", number_of_objects=" << std::to_string(numberOfObjects)
      << ", test=" << bool_toString(test)
      << ", negative=" << bool_toString(negative)
      << ", sequence=" << bool_toString(sequence) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
};
