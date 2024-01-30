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
 * @file PointInformation.cpp
 * @brief create an outgoing from data point in monitoring direction
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "PointMessage.h"
#include "object/DataPoint.h"

using namespace Remote::Message;

PointMessage::PointMessage(std::shared_ptr<Object::DataPoint> point)
    : OutgoingMessage(point), updated_at(0), duration({0}), time({0}) {
  causeOfTransmission = CS101_COT_SPONTANEOUS;
  updated_at = point->getUpdatedAt_ms();

  CP56Time2a_createFromMsTimestamp(&time, updated_at);

  switch (type) {
  // bool Single Point
  // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_SP_NA_1: {
    io = (InformationObject)SinglePointInformation_create(
        nullptr, informationObjectAddress, (bool)value.load(),
        static_cast<uint8_t>(quality.load()));
  } break;

    // bool Single Point + Extended Time
    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_SP_TB_1: {
    io = (InformationObject)SinglePointWithCP56Time2a_create(
        nullptr, informationObjectAddress, (bool)value.load(),
        static_cast<uint8_t>(quality.load()), &time);
  } break;

    // enum Double Point [INTERMEDIATE|ON|OFF|INDETERMINATE]
    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_DP_NA_1: {
    auto state = (DoublePointValue)value.load();
    io = (InformationObject)DoublePointInformation_create(
        nullptr, informationObjectAddress, state,
        static_cast<uint8_t>(quality.load()));
  } break;

    // enum Double Point [INTERMEDIATE|ON|OFF|INDETERMINATE] + Extended Time
    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_DP_TB_1: {
    auto state = (DoublePointValue)value.load();
    io = (InformationObject)DoublePointWithCP56Time2a_create(
        nullptr, informationObjectAddress, state,
        static_cast<uint8_t>(quality.load()), &time);
  } break;

    // int [-64,63] StepPosition (Trafo)
    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_ST_NA_1: {
    io = (InformationObject)StepPositionInformation_create(
        nullptr, informationObjectAddress, (int)value.load(), false,
        static_cast<uint8_t>(quality.load()));
  } break;

    // int [-64,63] StepPosition (Trafo) + Extended Time
    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_ST_TB_1: {
    io = (InformationObject)StepPositionWithCP56Time2a_create(
        nullptr, informationObjectAddress, (int)value.load(), false,
        static_cast<uint8_t>(quality.load()), &time);
  } break;

    // uint32_t [0,2^32] BitString 32bits
  case M_BO_NA_1: {
    // @todo what happens in case of bad quality ??
    io = (InformationObject)BitString32_create(
        nullptr, informationObjectAddress, (uint32_t)value.load());
  } break;

    // uint32_t [0,2^32] BitString 32bits + Extended Time
  case M_BO_TB_1: {
    // @todo what happens in case of bad quality ??
    io = (InformationObject)Bitstring32WithCP56Time2a_create(
        nullptr, informationObjectAddress, (uint32_t)value.load(), &time);
  } break;

    // float Measurement Value (NORMALIZED)
    // NORMALIZATION: from [-1.0f to +1.0f] encoded to int16 [-32.768‬
    // to 32.767] Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_NA_1: {
    io = (InformationObject)MeasuredValueNormalized_create(
        nullptr, informationObjectAddress, (float)value.load(),
        static_cast<uint8_t>(quality.load()));
  } break;

    // int16 Measurement Value (NORMALIZED) + Extended Time
    // NORMALIZATION: from [-1.0f to +1.0f] encoded to [-32.768‬ to 32.767]
    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_TD_1: {
    io = (InformationObject)MeasuredValueNormalizedWithCP56Time2a_create(
        nullptr, informationObjectAddress, (float)value.load(),
        static_cast<uint8_t>(quality.load()), &time);
  } break;

    // uint16 Measurement Value (SCALED)
    // SCALED: from [-65536 to +65535] encoded to [0 to 65535] via negative
    // values + 65535 Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_NB_1: {
    io = (InformationObject)MeasuredValueScaled_create(
        nullptr, informationObjectAddress, (int)value.load(),
        static_cast<uint8_t>(quality.load()));
  } break;

    // uint16 Measurement Value (SCALED) + Extended Time
    // SCALED: from [-65536 to +65535] encoded to [0 to 65535] via negative
    // values + 65535 Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_TE_1: {
    io = (InformationObject)MeasuredValueScaledWithCP56Time2a_create(
        nullptr, informationObjectAddress, (int)value.load(),
        static_cast<uint8_t>(quality.load()), &time);
  } break;

    // float (32bit-256) Measurement Value (SHORT)
    //  [-32.768‬ to 32.767]
    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_NC_1: {
    io = (InformationObject)MeasuredValueShort_create(
        nullptr, informationObjectAddress, (float)value.load(),
        static_cast<uint8_t>(quality.load()));
  } break;

    // float (32bit-256) Measurement Value (SHORT) + Extended Time
    //  [-32.768‬ to 32.767]
    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_TF_1: {
    io = (InformationObject)MeasuredValueShortWithCP56Time2a_create(
        nullptr, informationObjectAddress, (float)value.load(),
        static_cast<uint8_t>(quality.load()), &time);
  } break;

    // Encoded Counter Value
  case M_IT_NA_1: {
    // @todo what happens in case of bad quality ??
    // @todo flags and sequence number usage
    uint_fast8_t seqNumber = 0;
    bool hasCarry = ::test(quality.load(), Quality::Overflow),
         isAdjusted = false, isInvalid = is_any(quality.load());
    io = (InformationObject)BinaryCounterReading_create(
        nullptr, (int)value.load(), seqNumber, hasCarry, isAdjusted, isInvalid);
  } break;

    // Encoded Counter Value + Extended Timer
  case M_IT_TB_1: {
    // @todo what happens in case of bad quality ??
    // @todo flags and sequence number usage
    uint_fast8_t seqNumber = 0;
    bool hasCarry = ::test(quality.load(), Quality::Overflow),
         isAdjusted = false, isInvalid = is_any(quality.load());
    BinaryCounterReading _value = BinaryCounterReading_create(
        nullptr, (int)value.load(), seqNumber, hasCarry, isAdjusted, isInvalid);
    io = (InformationObject)IntegratedTotalsWithCP56Time2a_create(
        nullptr, informationObjectAddress, _value, &time);
  } break;

    // SingleEvent Protection Equipment + Extended Timer
    //@todo not yet supported - set event
  case M_EP_TD_1: {
    throw std::invalid_argument("Event messages not supported!");
    uint_fast32_t elapsedTime_ms = 0;
    CP16Time2a_setEplapsedTimeInMs(&duration, elapsedTime_ms);
    tSingleEvent event = IEC60870_EVENTSTATE_ON; // untested... lifetime ??
    io = (InformationObject)EventOfProtectionEquipmentWithCP56Time2a_create(
        nullptr, informationObjectAddress, &event, &duration, &time);
  } break;

    // StartEvent Protection Equipment + Extended Timer
    //@todo not yet supported - set event
  case M_EP_TE_1: {
    throw std::invalid_argument("Event messages not supported!");
    uint_fast32_t elapsedTime_ms = 0;
    CP16Time2a_setEplapsedTimeInMs(&duration, elapsedTime_ms);
    io = (InformationObject)
        PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(
            nullptr, informationObjectAddress, IEC60870_START_EVENT_GS,
            static_cast<uint8_t>(quality.load()), &duration, &time);
  } break;

    // OuputCircuitInfo Protection Equipment + Extended Timer
    //@todo not yet supported - set output curcuit info
  case M_EP_TF_1: {
    throw std::invalid_argument("Event messages not supported!");
    uint_fast32_t operatingTime_ms = 0;
    CP16Time2a_setEplapsedTimeInMs(&duration, operatingTime_ms);
    io = (InformationObject)PackedOutputCircuitInfoWithCP56Time2a_create(
        nullptr, informationObjectAddress, IEC60870_OUTPUT_CI_GC,
        static_cast<uint8_t>(quality.load()), &duration, &time);
  } break;

    // StatusAndStatusChangeDetection Single + Change Detection
    //@todo not yet supported - set sscd info
  case M_PS_NA_1: {
    throw std::invalid_argument("StatusAndStatusChangeDetection messages not "
                                "supported!");
    sStatusAndStatusChangeDetection sscd{0}; // untested... lifetime ??
    StatusAndStatusChangeDetection_setSTn(&sscd, 0);
    io = (InformationObject)PackedSinglePointWithSCD_create(
        nullptr, informationObjectAddress, &sscd,
        static_cast<uint8_t>(quality.load()));
  } break;

    // float Measurement Value (NORMALIZED) - Quality
  case M_ME_ND_1: {
    // @todo what happens in case of bad quality ??
    io = (InformationObject)MeasuredValueNormalizedWithoutQuality_create(
        nullptr, informationObjectAddress, (float)value.load());
  } break;

    // End of initialization
  case M_EI_NA_1: {
    throw std::invalid_argument("End of initialization is not a PointMessage!");
    informationObjectAddress = 0;
    io = (InformationObject)EndOfInitialization_create(
        nullptr, IEC60870_COI_REMOTE_RESET);
  } break;

  case S_IT_TC_1: {
    // @todo add IT messages
    throw std::invalid_argument("Integrated totals messages not supported!");
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
}

PointMessage::~PointMessage() {
  if (!io)
    return;

  switch (type) {
  case M_SP_NA_1:
    SinglePointInformation_destroy((SinglePointInformation)io);
    break;
  case M_SP_TB_1:
    SinglePointWithCP56Time2a_destroy((SinglePointWithCP56Time2a)io);
    break;
  case M_DP_NA_1:
    DoublePointInformation_destroy((DoublePointInformation)io);
    break;
  case M_DP_TB_1:
    DoublePointWithCP56Time2a_destroy((DoublePointWithCP56Time2a)io);
    break;
  case M_ST_NA_1:
    StepPositionInformation_destroy((StepPositionInformation)io);
    break;
  case M_ST_TB_1:
    StepPositionWithCP56Time2a_destroy((StepPositionWithCP56Time2a)io);
    break;
  case M_BO_NA_1:
    BitString32_destroy((BitString32)io);
    break;
  case M_BO_TB_1:
    Bitstring32WithCP56Time2a_destroy((Bitstring32WithCP56Time2a)io);
    break;
  case M_ME_NA_1:
    MeasuredValueNormalized_destroy((MeasuredValueNormalized)io);
    break;
  case M_ME_TD_1:
    MeasuredValueNormalizedWithCP56Time2a_destroy(
        (MeasuredValueNormalizedWithCP56Time2a)io);
    break;
  case M_ME_NB_1:
    MeasuredValueScaled_destroy((MeasuredValueScaled)io);
    break;
  case M_ME_TE_1:
    MeasuredValueScaledWithCP56Time2a_destroy(
        (MeasuredValueScaledWithCP56Time2a)io);
    break;
  case M_ME_NC_1:
    MeasuredValueShort_destroy((MeasuredValueShort)io);
    break;
  case M_ME_TF_1:
    MeasuredValueShortWithCP56Time2a_destroy(
        (MeasuredValueShortWithCP56Time2a)io);
    break;
  case M_IT_NA_1:
    IntegratedTotals_destroy((IntegratedTotals)io);
    break;
  case M_IT_TB_1:
    IntegratedTotalsWithCP56Time2a_destroy((IntegratedTotalsWithCP56Time2a)io);
    break;
  case M_EP_TD_1:
    EventOfProtectionEquipmentWithCP56Time2a_destroy(
        (EventOfProtectionEquipmentWithCP56Time2a)io);
    break;
  case M_EP_TE_1:
    PackedStartEventsOfProtectionEquipmentWithCP56Time2a_destroy(
        (PackedStartEventsOfProtectionEquipmentWithCP56Time2a)io);
    break;
  case M_EP_TF_1:
    PackedOutputCircuitInfoWithCP56Time2a_destroy(
        (PackedOutputCircuitInfoWithCP56Time2a)io);
    break;
  case M_PS_NA_1:
    PackedSinglePointWithSCD_destroy((PackedSinglePointWithSCD)io);
    break;
  case M_ME_ND_1:
    MeasuredValueNormalizedWithoutQuality_destroy(
        (MeasuredValueNormalizedWithoutQuality)io);
    break;
  default:
    std::cerr << "Unsupported type " << TypeID_toString(type) << std::endl;
  }
}
