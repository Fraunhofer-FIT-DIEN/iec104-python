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
#include "object/Information.h"

using namespace Remote::Message;

PointMessage::PointMessage(std::shared_ptr<Object::DataPoint> point)
    : OutgoingMessage(point) {
  causeOfTransmission = CS101_COT_SPONTANEOUS;

  switch (type) {

  // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_SP_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::SingleInfo>(info);
    io = (InformationObject)SinglePointInformation_create(
        nullptr, informationObjectAddress, i->isOn(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_SP_TB_1: {
    auto i = std::dynamic_pointer_cast<Object::SingleInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)SinglePointWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->isOn(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &time);
  } break;

    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_DP_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::DoubleInfo>(info);
    io = (InformationObject)DoublePointInformation_create(
        nullptr, informationObjectAddress, i->getState(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_DP_TB_1: {
    auto i = std::dynamic_pointer_cast<Object::DoubleInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)DoublePointWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getState(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &time);
  } break;

    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_ST_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::StepInfo>(info);
    io = (InformationObject)StepPositionInformation_create(
        nullptr, informationObjectAddress, i->getPosition().get(),
        i->isTransient(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // Valid cause of transmission: 2,3,5,11,12,20-36
  case M_ST_TB_1: {
    auto i = std::dynamic_pointer_cast<Object::StepInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)StepPositionWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getPosition().get(),
        i->isTransient(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &time);
  } break;

  case M_BO_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::BinaryInfo>(info);
    io = (InformationObject)BitString32_create(
        nullptr, informationObjectAddress, i->getBlob().get());
  } break;

  case M_BO_TB_1: {
    auto i = std::dynamic_pointer_cast<Object::BinaryInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)Bitstring32WithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getBlob().get(), &time);
  } break;

    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::NormalizedInfo>(info);
    io = (InformationObject)MeasuredValueNormalized_create(
        nullptr, informationObjectAddress, i->getActual().get(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_TD_1: {
    auto i = std::dynamic_pointer_cast<Object::NormalizedInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)MeasuredValueNormalizedWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getActual().get(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &time);
  } break;

    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_NB_1: {
    auto i = std::dynamic_pointer_cast<Object::ScaledInfo>(info);
    io = (InformationObject)MeasuredValueScaled_create(
        nullptr, informationObjectAddress, i->getActual().get(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_TE_1: {
    auto i = std::dynamic_pointer_cast<Object::ScaledInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)MeasuredValueScaledWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getActual().get(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &time);
  } break;

    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_NC_1: {
    auto i = std::dynamic_pointer_cast<Object::ShortInfo>(info);
    io = (InformationObject)MeasuredValueShort_create(
        nullptr, informationObjectAddress, i->getActual(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // Valid cause of transmission: 1,2,3,5,20-36
  case M_ME_TF_1: {
    auto i = std::dynamic_pointer_cast<Object::ShortInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    io = (InformationObject)MeasuredValueShortWithCP56Time2a_create(
        nullptr, informationObjectAddress, i->getActual(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &time);
  } break;

  case M_IT_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::BinaryCounterInfo>(info);
    auto q = std::get<BinaryCounterQuality>(i->getQuality());
    BinaryCounterReading _value = BinaryCounterReading_create(
        nullptr, i->getCounter(), i->getSequence().get(),
        ::test(q, BinaryCounterQuality::Carry),
        ::test(q, BinaryCounterQuality::Adjusted),
        ::test(q, BinaryCounterQuality::Invalid));
    io = (InformationObject)IntegratedTotals_create(
        nullptr, informationObjectAddress, _value);
  } break;

  case M_IT_TB_1: {
    auto i = std::dynamic_pointer_cast<Object::BinaryCounterInfo>(info);
    auto q = std::get<BinaryCounterQuality>(i->getQuality());
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    BinaryCounterReading _value = BinaryCounterReading_create(
        nullptr, i->getCounter(), i->getSequence().get(),
        ::test(q, BinaryCounterQuality::Carry),
        ::test(q, BinaryCounterQuality::Adjusted),
        ::test(q, BinaryCounterQuality::Invalid));
    io = (InformationObject)IntegratedTotalsWithCP56Time2a_create(
        nullptr, informationObjectAddress, _value, &time);
  } break;

  case M_EP_TD_1: {
    auto i =
        std::dynamic_pointer_cast<Object::ProtectionEquipmentEventInfo>(info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    sCP16Time2a elapsed{};
    CP16Time2a_setEplapsedTimeInMs(&elapsed, i->getElapsed_ms().get());
    tSingleEvent event =
        ((static_cast<uint8_t>(i->getState()) & 0b00000111) |
         (static_cast<uint8_t>(std::get<Quality>(i->getQuality())) &
          0b11111000));
    io = (InformationObject)EventOfProtectionEquipmentWithCP56Time2a_create(
        nullptr, informationObjectAddress, &event, &elapsed, &time);
  } break;

  case M_EP_TE_1: {
    auto i =
        std::dynamic_pointer_cast<Object::ProtectionEquipmentStartEventsInfo>(
            info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    sCP16Time2a elapsed{};
    CP16Time2a_setEplapsedTimeInMs(&elapsed, i->getRelayDuration_ms().get());
    io = (InformationObject)
        PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(
            nullptr, informationObjectAddress,
            static_cast<uint8_t>(i->getEvents()),
            static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &elapsed,
            &time);
  } break;

  case M_EP_TF_1: {
    auto i =
        std::dynamic_pointer_cast<Object::ProtectionEquipmentOutputCircuitInfo>(
            info);
    sCP56Time2a time{};
    from_time_point(&time, i->getRecordedAt().value_or(i->getProcessedAt()));
    sCP16Time2a elapsed{};
    CP16Time2a_setEplapsedTimeInMs(&elapsed, i->getRelayOperating_ms().get());
    io = (InformationObject)PackedOutputCircuitInfoWithCP56Time2a_create(
        nullptr, informationObjectAddress,
        static_cast<uint8_t>(i->getCircuits()),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &elapsed,
        &time);
  } break;

  case M_PS_NA_1: {
    auto i = std::dynamic_pointer_cast<Object::StatusWithChangeDetection>(info);
    sStatusAndStatusChangeDetection sscd{};
    auto status = static_cast<uint16_t>(i->getStatus());
    auto changed = static_cast<uint16_t>(i->getChanged());
    sscd.encodedValue[0] = (status >> 0) & 0b11111111;
    sscd.encodedValue[1] = (status >> 8) & 0b11111111;
    sscd.encodedValue[2] = (changed >> 0) & 0b11111111;
    sscd.encodedValue[3] = (changed >> 8) & 0b11111111;
    io = (InformationObject)PackedSinglePointWithSCD_create(
        nullptr, informationObjectAddress, &sscd,
        static_cast<uint8_t>(std::get<Quality>(i->getQuality())));
  } break;

    // float Measurement Value (NORMALIZED) - Quality
  case M_ME_ND_1: {
    auto i = std::dynamic_pointer_cast<Object::NormalizedInfo>(info);
    io = (InformationObject)MeasuredValueNormalizedWithoutQuality_create(
        nullptr, informationObjectAddress, i->getActual().get());
  } break;

    // End of initialization
  case M_EI_NA_1: {
    // todo remove??
    throw std::invalid_argument("End of initialization is not a PointMessage!");
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
