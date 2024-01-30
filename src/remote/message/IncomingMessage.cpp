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
#include "remote/Helper.h"

using namespace Remote::Message;

IncomingMessage::IncomingMessage(CS101_ASDU packet,
                                 CS101_AppLayerParameters app_layer_parameters)
    : IMessageInterface(), asdu(packet), parameters(app_layer_parameters),
      position(0), positionReset(true), positionValid(false),
      numberOfObject(0) {
  if (packet) {
    extractMetaData();
    first();
  }
  DEBUG_PRINT(Debug::Message, "Created (incoming)");
}

IncomingMessage::~IncomingMessage() {
  if (io) {
    InformationObject_destroy(io);
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

std::uint_fast64_t IncomingMessage::getUpdatedAt() const { return updatedAt; }

void IncomingMessage::first() {
  {
    std::lock_guard<Module::GilAwareMutex> const lock(position_mutex);

    positionReset = true;
    position = 0;
    positionValid = (numberOfObject > 0);
  }

  if (positionValid)
    extractInformationObject();
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
    extractInformationObject();

  return positionValid;
}

void IncomingMessage::extractInformationObject() {
  std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

  if (io)
    InformationObject_destroy(io);

  io = CS101_ASDU_getElement(asdu, position);
  informationObjectAddress =
      (io == nullptr) ? 0 : InformationObject_getObjectAddress(io);
  value = 0;
  quality = Quality::Invalid;

  if (positionValid && informationObjectAddress) {
    bool transient = false;
    // IEC 60870-5-104 standard uses only messages without timestamp or with
    // CP56Time2a timestamp
    CP16Time2a elapsed = nullptr;
    CP56Time2a timestamp56 = nullptr;

    switch (type) {
    /**
     * MONITORING
     */

    // s->c: bool Single Point
    case M_SP_NA_1: {
      value = SinglePointInformation_getValue((SinglePointInformation)io);
      quality = static_cast<Quality>(
          SinglePointInformation_getQuality((SinglePointInformation)io));
    } break;

      // s->c: bool Single Point + Extended Time
    case M_SP_TB_1: {
      value = SinglePointInformation_getValue((SinglePointInformation)io);
      quality = static_cast<Quality>(
          SinglePointInformation_getQuality((SinglePointInformation)io));
      timestamp56 =
          SinglePointWithCP56Time2a_getTimestamp((SinglePointWithCP56Time2a)io);
    } break;

      // s->c: enum Double Point [INTERMEDIATE|ON|OFF|INDETERMINATE]
    case M_DP_NA_1: {
      DoublePointValue dpv1 =
          DoublePointInformation_getValue((DoublePointInformation)io);
      value = dpv1;
      quality = static_cast<Quality>(
          DoublePointInformation_getQuality((DoublePointInformation)io));
    } break;

      // s->c: enum Double Point [INTERMEDIATE|ON|OFF|INDETERMINATE] + Extended
      // Time
    case M_DP_TB_1: {
      DoublePointValue dpv3 =
          DoublePointInformation_getValue((DoublePointInformation)io);
      value = dpv3;
      quality = static_cast<Quality>(
          DoublePointInformation_getQuality((DoublePointInformation)io));
      timestamp56 =
          DoublePointWithCP56Time2a_getTimestamp((DoublePointWithCP56Time2a)io);
    } break;

      // s->c: int [-64,63] StepPosition (Trafo)
    case M_ST_NA_1: {
      //@todo getObjectAddress ??? transient ???
      uint_fast16_t objAddr1 =
          StepPositionInformation_getObjectAddress((StepPositionInformation)io);
      value = StepPositionInformation_getValue((StepPositionInformation)io);
      quality = static_cast<Quality>(
          StepPositionInformation_getQuality((StepPositionInformation)io));
      transient =
          StepPositionInformation_isTransient((StepPositionInformation)io);
    } break;

      // s->c: int [-64,63] StepPosition (Trafo) + Extended Time
    case M_ST_TB_1: {
      //@todo getObjectAddress ??? transient ???
      uint_fast16_t objAddr3 =
          StepPositionInformation_getObjectAddress((StepPositionInformation)io);
      value = StepPositionInformation_getValue((StepPositionInformation)io);
      quality = static_cast<Quality>(
          StepPositionInformation_getQuality((StepPositionInformation)io));
      transient =
          StepPositionInformation_isTransient((StepPositionInformation)io);
      timestamp56 = StepPositionWithCP56Time2a_getTimestamp(
          (StepPositionWithCP56Time2a)io);
    } break;

      // s->c: [0,2^32] BitString 32bits
    case M_BO_NA_1: {
      //@todo usage? conversion?
      value = BitString32_getValue((BitString32)io);
      quality = static_cast<Quality>(BitString32_getQuality((BitString32)io));
    } break;

      // s->c: [0,2^32] BitString 32bits + Extended Time
    case M_BO_TB_1: {
      //@todo usage? conversion?
      value = BitString32_getValue((BitString32)io);
      quality = static_cast<Quality>(BitString32_getQuality((BitString32)io));
      timestamp56 =
          Bitstring32WithCP56Time2a_getTimestamp((Bitstring32WithCP56Time2a)io);
    } break;

      // s->c: float Measurement Value (NORMALIZED)
    case M_ME_NA_1: {
      value = MeasuredValueNormalized_getValue((MeasuredValueNormalized)io);
      quality = static_cast<Quality>(
          MeasuredValueNormalized_getQuality((MeasuredValueNormalized)io));
    } break;

      // s->c: float Measurement Value (NORMALIZED) + Extended Time
    case M_ME_TD_1: {
      value = MeasuredValueNormalized_getValue((MeasuredValueNormalized)io);
      quality = static_cast<Quality>(
          MeasuredValueNormalized_getQuality((MeasuredValueNormalized)io));
      timestamp56 = MeasuredValueNormalizedWithCP56Time2a_getTimestamp(
          (MeasuredValueNormalizedWithCP56Time2a)io);
    } break;

      // s->c: int Measurement Value (SCALED)
    case M_ME_NB_1: {
      value = MeasuredValueScaled_getValue((MeasuredValueScaled)io);
      quality = static_cast<Quality>(
          MeasuredValueScaled_getQuality((MeasuredValueScaled)io));
    } break;

      // s->c: int Measurement Value (SCALED) + Extended Time
    case M_ME_TE_1: {
      value = MeasuredValueScaled_getValue((MeasuredValueScaled)io);
      quality = static_cast<Quality>(
          MeasuredValueScaled_getQuality((MeasuredValueScaled)io));
      timestamp56 = MeasuredValueScaledWithCP56Time2a_getTimestamp(
          (MeasuredValueScaledWithCP56Time2a)io);
    } break;

      // s->c: float Measurement Value (SHORT)
    case M_ME_NC_1: {
      //@todo not normalized or scaled ?
      value = MeasuredValueShort_getValue((MeasuredValueShort)io);
      quality = static_cast<Quality>(
          MeasuredValueShort_getQuality((MeasuredValueShort)io));
    } break;

      // s->c: float Measurement Value (SHORT) + Extended Time
    case M_ME_TF_1: {
      value = MeasuredValueShort_getValue((MeasuredValueShort)io);
      quality = static_cast<Quality>(
          MeasuredValueShort_getQuality((MeasuredValueShort)io));
      timestamp56 = MeasuredValueShortWithCP56Time2a_getTimestamp(
          (MeasuredValueShortWithCP56Time2a)io);
    } break;

      // s->c: Encoded Counter Value
    case M_IT_NA_1: {
      //@todo usecase? value conversion?
      //@todo what about quality ?
      BinaryCounterReading bcr1 = IntegratedTotals_getBCR((IntegratedTotals)io);
      value = (double)(bcr1->encodedValue[0] +
                       ((uint_fast64_t)bcr1->encodedValue[1] << 8) +
                       ((uint_fast64_t)bcr1->encodedValue[2] << 16) +
                       ((uint_fast64_t)bcr1->encodedValue[3] << 24) +
                       ((uint_fast64_t)bcr1->encodedValue[4] << 32));
      quality.store(Quality::None);
    } break;

      // s->c: Encoded Counter Value + Extended Timer
    case M_IT_TB_1: {
      //@todo support BCR, usecase? value?
      //@todo what about quality ?
      BinaryCounterReading bcr3 = IntegratedTotals_getBCR((IntegratedTotals)io);
      value = (double)(bcr3->encodedValue[0] +
                       ((uint_fast64_t)bcr3->encodedValue[1] << 8) +
                       ((uint_fast64_t)bcr3->encodedValue[2] << 16) +
                       ((uint_fast64_t)bcr3->encodedValue[3] << 24) +
                       ((uint_fast64_t)bcr3->encodedValue[4] << 32));
      timestamp56 = IntegratedTotalsWithCP56Time2a_getTimestamp(
          (IntegratedTotalsWithCP56Time2a)io);
      quality.store(Quality::None);
    } break;

      // s->c: SingleEvent Protection Equipment + Extended Timer
    case M_EP_TD_1: {
      //@todo usecase? handle event?? value?
      //@todo what about quality ?
      SingleEvent ev2 = EventOfProtectionEquipmentWithCP56Time2a_getEvent(
          (EventOfProtectionEquipmentWithCP56Time2a)io);
      elapsed = EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
          (EventOfProtectionEquipmentWithCP56Time2a)io);
      timestamp56 = EventOfProtectionEquipmentWithCP56Time2a_getTimestamp(
          (EventOfProtectionEquipmentWithCP56Time2a)io);
      quality.store(Quality::None);
    } break;

      // s->c: StartEvent Protection Equipment + Extended Timer
    case M_EP_TE_1: {
      //@todo usecase? handle event?? value?
      StartEvent se2 =
          PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(
              (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io);
      elapsed =
          PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
              (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io);
      quality = static_cast<Quality>(
          PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(
              (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io));
      timestamp56 =
          PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp(
              (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io);
    } break;

      // s->c: OuputCircuitInfo Protection Equipment + Extended Timer
    case M_EP_TF_1: {
      //@todo usecase? handle event?? value?
      OutputCircuitInfo oci2 = PackedOutputCircuitInfoWithCP56Time2a_getOCI(
          (PackedOutputCircuitInfoWithCP56Time2a)io);
      elapsed = PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime(
          (PackedOutputCircuitInfoWithCP56Time2a)io);
      quality =
          static_cast<Quality>(PackedOutputCircuitInfoWithCP56Time2a_getQuality(
              (PackedOutputCircuitInfoWithCP56Time2a)io));
      timestamp56 = PackedOutputCircuitInfoWithCP56Time2a_getTimestamp(
          (PackedOutputCircuitInfoWithCP56Time2a)io);
    } break;

      // s->c: StatusAndStatusChangeDetection Single + Change Detection
    case M_PS_NA_1: {
      //@todo usecase? decoded value?
      StatusAndStatusChangeDetection sscd =
          PackedSinglePointWithSCD_getSCD((PackedSinglePointWithSCD)io);
      value = (double)(sscd->encodedValue[0] +
                       ((uint_fast64_t)sscd->encodedValue[1] << 8) +
                       ((uint_fast64_t)sscd->encodedValue[2] << 16) +
                       ((uint_fast64_t)sscd->encodedValue[3] << 24));
      quality = static_cast<Quality>(
          PackedSinglePointWithSCD_getQuality((PackedSinglePointWithSCD)io));
    } break;

      // s->c: float Measurement Value (NORMALIZED) - Quality
    case M_ME_ND_1: {
      //@todo usecase?
      //@todo what about quality ?
      value = MeasuredValueNormalizedWithoutQuality_getValue(
          (MeasuredValueNormalizedWithoutQuality)io);
      quality.store(Quality::None);
    } break;

      /**
       * CONTROL
       */

      // c->s: bool
    case C_SC_NA_1: {
      //@todo state vs selected? what is qu,selected?
      //@todo what about quality ?
      int qu1 = SingleCommand_getQU((SingleCommand)io);
      selectFlag = SingleCommand_isSelect((SingleCommand)io);
      value = SingleCommand_getState((SingleCommand)io);
      if (value == 0 || value == 1) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: bool + Extended Time
    case C_SC_TA_1: {
      //@todo state vs selected? what is qu,selected?
      //@todo what about quality ?
      int qu2 = SingleCommand_getQU((SingleCommand)io);
      selectFlag = SingleCommand_isSelect((SingleCommand)io);
      value = SingleCommand_getState((SingleCommand)io);
      timestamp56 = SingleCommandWithCP56Time2a_getTimestamp(
          (SingleCommandWithCP56Time2a)io);
      if (value == 0 || value == 1) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: int /enum??
    case C_DC_NA_1: {
      //@todo what is qu,selected?
      //@todo what about quality / use qu for command ?
      int qu3 = DoubleCommand_getQU((DoubleCommand)io);
      selectFlag = DoubleCommand_isSelect((DoubleCommand)io);
      value = DoubleCommand_getState((DoubleCommand)io);
      if (value == 0 || value == 1 || value == 2 || value == 3) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: int /enum? + Extended Time
    case C_DC_TA_1: {
      //@todo what is qu,selected?
      //@todo what about quality / use qu for command ?
      int qu4 =
          DoubleCommandWithCP56Time2a_getQU((DoubleCommandWithCP56Time2a)io);
      selectFlag =
          DoubleCommandWithCP56Time2a_isSelect((DoubleCommandWithCP56Time2a)io);
      value =
          DoubleCommandWithCP56Time2a_getState((DoubleCommandWithCP56Time2a)io);
      // timestamp56 =
      // DoubleCommandWithCP56Time2a_getTimestamp((DoubleCommandWithCP56Time2a)io);
      // not found
      if (value == 0 || value == 1 || value == 2 || value == 3) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Regulation Step Command
    case C_RC_NA_1: {
      //@todo what about quality / use qu for command ?
      int qu5 = StepCommand_getQU((StepCommand)io);
      selectFlag = StepCommand_isSelect((StepCommand)io);
      StepCommandValue scv1 = StepCommand_getState((StepCommand)io);
      value = scv1;
      if (value == 1 || value == 2) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Regulation Step Command + Extended Time
    case C_RC_TA_1: {
      //@todo what about quality / use qu for command ?
      int qu6 = StepCommandWithCP56Time2a_getQU((StepCommandWithCP56Time2a)io);
      selectFlag = StepCommand_isSelect((StepCommand)io);
      StepCommandValue scv2 =
          StepCommandWithCP56Time2a_getState((StepCommandWithCP56Time2a)io);
      value = scv2;
      // timestamp56 =
      // StepCommandWithCP56Time2a_((StepCommandWithCP56Time2a)io); not found
      if (value == 1 || value == 2) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Setpoint Command (NORMALIZED)
    case C_SE_NA_1: {
      //@todo what about quality / use qu for command ?
      int ql1 = SetpointCommandNormalized_getQL((SetpointCommandNormalized)io);
      selectFlag =
          SetpointCommandNormalized_isSelect((SetpointCommandNormalized)io);
      value = SetpointCommandNormalized_getValue((SetpointCommandNormalized)io);
      if (value >= -1 && value <= 1) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Setpoint Command (NORMALIZED) * Extended Time
    case C_SE_TA_1: {
      //@todo what about quality / use qu for command ?
      int ql2 = SetpointCommandNormalizedWithCP56Time2a_getQL(
          (SetpointCommandNormalizedWithCP56Time2a)io);
      selectFlag = SetpointCommandNormalizedWithCP56Time2a_isSelect(
          (SetpointCommandNormalizedWithCP56Time2a)io);
      value = SetpointCommandNormalizedWithCP56Time2a_getValue(
          (SetpointCommandNormalizedWithCP56Time2a)io);
      // timestamp56 =
      // SetpointCommandNormalizedWithCP56Time2a_((SetpointCommandNormalizedWithCP56Time2a)io);
      if (value >= -1 && value <= 1) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Setpoint Command (SCALED)
    case C_SE_NB_1: {
      //@todo what about quality / use qu for command ?
      int ql3 = SetpointCommandScaled_getQL((SetpointCommandScaled)io);
      selectFlag = SetpointCommandScaled_isSelect((SetpointCommandScaled)io);
      value = SetpointCommandScaled_getValue((SetpointCommandScaled)io);
      if (value >= -65536. && value <= 65535.) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Setpoint Command (SCALED) + Extended Time
    case C_SE_TB_1: {
      //@todo what about quality / use qu for command ?
      int ql4 = SetpointCommandScaledWithCP56Time2a_getQL(
          (SetpointCommandScaledWithCP56Time2a)io);
      selectFlag = SetpointCommandScaledWithCP56Time2a_isSelect(
          (SetpointCommandScaledWithCP56Time2a)io);
      value = SetpointCommandScaledWithCP56Time2a_getValue(
          (SetpointCommandScaledWithCP56Time2a)io);
      // timestamp56 =
      // SetpointCommandScaledWithCP56Time2a_((SetpointCommandScaledWithCP56Time2a)io);
      if (value >= -65536. && value <= 65535.) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Setpoint Command (SHORT)
    case C_SE_NC_1: {
      //@todo what about quality / use qu for command ?
      int ql5 = SetpointCommandShort_getQL((SetpointCommandShort)io);
      selectFlag = SetpointCommandShort_isSelect((SetpointCommandShort)io);
      value = SetpointCommandShort_getValue((SetpointCommandShort)io);
      if (value >= -16777216. && value <= 16777215.) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: Setpoint Command (SHORT) + Extended Time
    case C_SE_TC_1: {
      //@todo what about quality / use qu for command ?
      int ql6 = SetpointCommandShortWithCP56Time2a_getQL(
          (SetpointCommandShortWithCP56Time2a)io);
      selectFlag = SetpointCommandShortWithCP56Time2a_isSelect(
          (SetpointCommandShortWithCP56Time2a)io);
      value = SetpointCommandShortWithCP56Time2a_getValue(
          (SetpointCommandShortWithCP56Time2a)io);
      // timestamp56 =
      // SetpointCommandShortWithCP56Time2a_((SetpointCommandShortWithCP56Time2a)io);
      if (value >= -16777216. && value <= 16777215.) {
        quality.store(Quality::None);
      }
    } break;

      // c->s: BitString 32bit Command
    case C_BO_NA_1: {
      //@todo what about quality / use qu for command ?
      value = (double)Bitstring32Command_getValue((Bitstring32Command)io);
      quality.store(Quality::None);
    } break;

      // c->s: BitString 32bit Command + Extended Time
    case C_BO_TA_1: {
      //@todo what about quality / use qu for command ?
      value = (double)Bitstring32CommandWithCP56Time2a_getValue(
          (Bitstring32CommandWithCP56Time2a)io);
      timestamp56 = Bitstring32CommandWithCP56Time2a_getTimestamp(
          (Bitstring32CommandWithCP56Time2a)io);
      quality.store(Quality::None);
    } break;

      // c->s: READ Command
    case C_RD_NA_1: {
      // accept incoming message instances within read commands
      quality.store(Quality::None);
    } break;

      /**
       * Unhandled message
       */

    default:
      std::cerr << "[c104.IncomingMessage.extract] Unsupported type "
                << TypeID_toString(type) << std::endl;
    }

    if (timestamp56) {
      updatedAt = CP56Time2a_toMsTimestamp(timestamp56);
    }

    // detect NaN values
    if (std::isnan(value.load())) {
      // value.store(0);
      // quality.store(Quality::Invalid);
      DEBUG_PRINT(Debug::Point, "IncomingMessage.extract] detected NaN value "
                                "in incoming message at IOA " +
                                    std::to_string(informationObjectAddress));
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
  if ((type < C_SC_NA_1 && type > C_SE_NC_1) ||
      (type < C_SC_TA_1 && type > C_SE_TC_1)) {
    DEBUG_PRINT(Debug::Message,
                "IncomingMessage.isSelectCommand] point at IOA " +
                    std::to_string(informationObjectAddress) + " of TypeID " +
                    TypeID_toString(type) + " does not carry a SELECT flag");
  }
  return selectFlag;
}
