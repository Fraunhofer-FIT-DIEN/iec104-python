/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied IInformation Technology
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
 * @file Type.cpp
 * @brief protocol specific helper function for information objects
 *
 * @package iec104-python
 * @namespace Transformer
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "transformer/Information.h"
#include "bitflag.h"
#include "object/information/BinaryCmd.h"
#include "object/information/BinaryInfo.h"
#include "object/information/DoubleCmd.h"
#include "object/information/DoubleInfo.h"
#include "object/information/EquipmentProtectionEvent.h"
#include "object/information/EquipmentProtectionOutputCircuitEvent.h"
#include "object/information/EquipmentProtectionStartEvents.h"
#include "object/information/Generic.h"
#include "object/information/IntegratedTotalInfo.h"
#include "object/information/NormalizedCmd.h"
#include "object/information/NormalizedInfo.h"
#include "object/information/ScaledCmd.h"
#include "object/information/ScaledInfo.h"
#include "object/information/ShortCmd.h"
#include "object/information/ShortInfo.h"
#include "object/information/SingleCmd.h"
#include "object/information/SingleInfo.h"
#include "object/information/StatusWithChangeDetection.h"
#include "object/information/StepCmd.h"
#include "object/information/StepInfo.h"

using namespace Object::Information;

std::shared_ptr<IInformation>
Transformer::fromInformationObject(const InformationObject io) {
  const auto type = InformationObject_getType(io);
  switch (type) {
  case M_SP_NA_1:
  case M_SP_TB_1: {
    const auto timestamp =
        (type == M_SP_TB_1)
            ? std::make_optional(
                  Object::DateTime(SinglePointWithCP56Time2a_getTimestamp(
                      reinterpret_cast<SinglePointWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<SinglePointInformation>(io);
    return std::make_shared<SingleInfo>(
        SinglePointInformation_getValue(_io),
        static_cast<Quality>(SinglePointInformation_getQuality(_io)), timestamp,
        true);
  } break;
  case C_SC_NA_1:
  case C_SC_TA_1: {
    const auto timestamp =
        (type == C_SC_TA_1)
            ? std::make_optional(
                  Object::DateTime(SingleCommandWithCP56Time2a_getTimestamp(
                      reinterpret_cast<SingleCommandWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<SingleCommand>(io);
    return std::make_shared<SingleCmd>(
        SingleCommand_getState(_io), SingleCommand_isSelect(_io),
        static_cast<CS101_QualifierOfCommand>(SingleCommand_getQU(_io)),
        timestamp, true);
  } break;
  case M_DP_NA_1:
  case M_DP_TB_1: {
    const auto timestamp =
        (type == M_DP_TB_1)
            ? std::make_optional(
                  Object::DateTime(DoublePointWithCP56Time2a_getTimestamp(
                      reinterpret_cast<DoublePointWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<DoublePointInformation>(io);
    return std::make_shared<DoubleInfo>(
        DoublePointInformation_getValue(_io),
        static_cast<Quality>(DoublePointInformation_getQuality(_io)), timestamp,
        true);
  } break;
  case C_DC_NA_1:
  case C_DC_TA_1: {
    const auto timestamp =
        (type == C_DC_TA_1)
            ? std::make_optional(
                  Object::DateTime(DoubleCommandWithCP56Time2a_getTimestamp(
                      reinterpret_cast<DoubleCommandWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<DoubleCommand>(io);
    return std::make_shared<DoubleCmd>(
        static_cast<DoublePointValue>(DoubleCommand_getState(_io)),
        DoubleCommand_isSelect(_io),
        static_cast<CS101_QualifierOfCommand>(DoubleCommand_getQU(_io)),
        timestamp, true);
  } break;
  case M_ST_NA_1:
  case M_ST_TB_1: {
    const auto timestamp =
        (type == M_ST_TB_1)
            ? std::make_optional(
                  Object::DateTime(StepPositionWithCP56Time2a_getTimestamp(
                      reinterpret_cast<StepPositionWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<StepPositionInformation>(io);
    return std::make_shared<StepInfo>(
        LimitedInt7(StepPositionInformation_getValue(_io)),
        StepPositionInformation_isTransient(_io),
        static_cast<Quality>(StepPositionInformation_getQuality(_io)),
        timestamp, true);
  } break;
  case C_RC_NA_1:
  case C_RC_TA_1: {
    const auto timestamp =
        (type == C_RC_TA_1)
            ? std::make_optional(
                  Object::DateTime(StepCommandWithCP56Time2a_getTimestamp(
                      reinterpret_cast<StepCommandWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<StepCommand>(io);
    return std::make_shared<StepCmd>(
        static_cast<StepCommandValue>(StepCommand_getState(_io)),
        StepCommand_isSelect(_io),
        static_cast<CS101_QualifierOfCommand>(StepCommand_getQU(_io)),
        timestamp, true);
  } break;
  case M_ME_ND_1: {
    // todo support missing quality
    return std::make_shared<NormalizedInfo>(
        NormalizedFloat(MeasuredValueNormalizedWithoutQuality_getValue(
            reinterpret_cast<MeasuredValueNormalizedWithoutQuality>(io))),
        Quality::None, std::nullopt, true);
  } break;
  case M_ME_NA_1:
  case M_ME_TD_1: {
    const auto timestamp =
        (type == M_ME_TD_1)
            ? std::make_optional(Object::DateTime(
                  MeasuredValueNormalizedWithCP56Time2a_getTimestamp(
                      reinterpret_cast<MeasuredValueNormalizedWithCP56Time2a>(
                          io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<MeasuredValueNormalized>(io);
    return std::make_shared<NormalizedInfo>(
        NormalizedFloat(MeasuredValueNormalized_getValue(_io)),
        static_cast<Quality>(MeasuredValueNormalized_getQuality(_io)),
        timestamp, true);
  } break;
  case C_SE_NA_1:
  case C_SE_TA_1: {
    const auto timestamp =
        (type == C_SE_TA_1)
            ? std::make_optional(Object::DateTime(
                  SetpointCommandNormalizedWithCP56Time2a_getTimestamp(
                      reinterpret_cast<SetpointCommandNormalizedWithCP56Time2a>(
                          io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<SetpointCommandNormalized>(io);
    return std::make_shared<NormalizedCmd>(
        NormalizedFloat(SetpointCommandNormalized_getValue(_io)),
        SetpointCommandNormalized_isSelect(_io),
        LimitedUInt7(
            static_cast<uint32_t>(SetpointCommandNormalized_getQL(_io))),
        timestamp, true);
  } break;
  case M_ME_NB_1:
  case M_ME_TE_1: {
    const auto timestamp =
        (type == M_ME_TE_1)
            ? std::make_optional(Object::DateTime(
                  MeasuredValueScaledWithCP56Time2a_getTimestamp(
                      reinterpret_cast<MeasuredValueScaledWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<MeasuredValueScaled>(io);
    return std::make_shared<ScaledInfo>(
        LimitedInt16(MeasuredValueScaled_getValue(_io)),
        static_cast<Quality>(MeasuredValueScaled_getQuality(_io)), timestamp,
        true);
  } break;
  case C_SE_NB_1:
  case C_SE_TB_1: {
    const auto timestamp =
        (type == C_SE_TB_1)
            ? std::make_optional(Object::DateTime(
                  SetpointCommandScaledWithCP56Time2a_getTimestamp(
                      reinterpret_cast<SetpointCommandScaledWithCP56Time2a>(
                          io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<SetpointCommandScaled>(io);
    return std::make_shared<ScaledCmd>(
        LimitedInt16(SetpointCommandScaled_getValue(_io)),
        SetpointCommandScaled_isSelect(_io),
        LimitedUInt7(static_cast<uint32_t>(SetpointCommandScaled_getQL(_io))),
        timestamp, true);
  } break;
  case M_ME_NC_1:
  case M_ME_TF_1: {
    const auto timestamp =
        (type == M_ME_TF_1)
            ? std::make_optional(Object::DateTime(
                  MeasuredValueShortWithCP56Time2a_getTimestamp(
                      reinterpret_cast<MeasuredValueShortWithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<MeasuredValueShort>(io);
    return std::make_shared<ShortInfo>(
        MeasuredValueShort_getValue(_io),
        static_cast<Quality>(MeasuredValueShort_getQuality(_io)), timestamp,
        true);
  } break;
  case C_SE_NC_1:
  case C_SE_TC_1: {
    const auto timestamp =
        (type == C_SE_TC_1)
            ? std::make_optional(Object::DateTime(
                  SetpointCommandShortWithCP56Time2a_getTimestamp(
                      reinterpret_cast<SetpointCommandShortWithCP56Time2a>(
                          io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<SetpointCommandShort>(io);
    return std::make_shared<ShortCmd>(
        SetpointCommandShort_getValue(_io), SetpointCommandShort_isSelect(_io),
        LimitedUInt7(static_cast<uint32_t>(SetpointCommandShort_getQL(_io))),
        timestamp, true);
  } break;
  case M_BO_NA_1:
  case M_BO_TB_1: {
    const auto timestamp =
        (type == M_BO_TB_1)
            ? std::make_optional(
                  Object::DateTime(Bitstring32WithCP56Time2a_getTimestamp(
                      reinterpret_cast<Bitstring32WithCP56Time2a>(io))))
            : std::nullopt;
    const auto _io = reinterpret_cast<BitString32>(io);
    return std::make_shared<BinaryInfo>(
        Byte32(BitString32_getValue(_io)),
        static_cast<Quality>(BitString32_getQuality(_io)), timestamp, true);
  } break;
  case C_BO_NA_1:
  case C_BO_TA_1: {
    const auto timestamp =
        (type == C_BO_TA_1)
            ? std::make_optional(Object::DateTime(
                  Bitstring32CommandWithCP56Time2a_getTimestamp(
                      reinterpret_cast<Bitstring32CommandWithCP56Time2a>(io))))
            : std::nullopt;
    return std::make_shared<BinaryCmd>(
        Byte32(Bitstring32Command_getValue(
            reinterpret_cast<Bitstring32Command>(io))),
        timestamp, true);
  } break;
  case M_IT_NA_1:
  case M_IT_TB_1: {
    const auto timestamp =
        (type == M_IT_TB_1)
            ? std::make_optional(
                  Object::DateTime(IntegratedTotalsWithCP56Time2a_getTimestamp(
                      reinterpret_cast<IntegratedTotalsWithCP56Time2a>(io))))
            : std::nullopt;
    BinaryCounterReading bcr1 =
        IntegratedTotals_getBCR(reinterpret_cast<IntegratedTotals>(io));
    return std::make_shared<BinaryCounterInfo>(
        BinaryCounterReading_getValue(bcr1),
        LimitedUInt5(static_cast<uint32_t>(
            BinaryCounterReading_getSequenceNumber(bcr1))),
        static_cast<BinaryCounterQuality>(bcr1->encodedValue[4] & 0b11100000),
        timestamp, true);
  } break;
  case M_EP_TD_1: {
    const auto _io =
        reinterpret_cast<EventOfProtectionEquipmentWithCP56Time2a>(io);
    SingleEvent single_event =
        EventOfProtectionEquipmentWithCP56Time2a_getEvent(_io);
    return std::make_shared<ProtectionEquipmentEventInfo>(
        static_cast<EventState>(*single_event & 0b00000111),
        LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
            EventOfProtectionEquipmentWithCP56Time2a_getElapsedTime(_io))),
        static_cast<Quality>(*single_event & 0b11111000),
        Object::DateTime(
            EventOfProtectionEquipmentWithCP56Time2a_getTimestamp(_io)),
        true);
  } break;
  case M_EP_TE_1: {
    const auto _io =
        reinterpret_cast<PackedStartEventsOfProtectionEquipmentWithCP56Time2a>(
            io);
    return std::make_shared<ProtectionEquipmentStartEventsInfo>(
        static_cast<StartEvents>(
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getEvent(_io) &
            0b00111111),
        LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getElapsedTime(
                _io))),
        static_cast<Quality>(
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getQuality(
                _io)),
        Object::DateTime(
            PackedStartEventsOfProtectionEquipmentWithCP56Time2a_getTimestamp(
                _io)),
        true);
  } break;
  case M_EP_TF_1: {
    const auto _io =
        reinterpret_cast<PackedOutputCircuitInfoWithCP56Time2a>(io);
    return std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        static_cast<OutputCircuits>(
            PackedOutputCircuitInfoWithCP56Time2a_getOCI(_io) & 0b00001111),
        LimitedUInt16(CP16Time2a_getEplapsedTimeInMs(
            PackedOutputCircuitInfoWithCP56Time2a_getOperatingTime(_io))),
        static_cast<Quality>(
            PackedOutputCircuitInfoWithCP56Time2a_getQuality(_io)),
        Object::DateTime(
            PackedOutputCircuitInfoWithCP56Time2a_getTimestamp(_io)),
        true);
  } break;
  case M_PS_NA_1: {
    const auto _io = reinterpret_cast<PackedSinglePointWithSCD>(io);
    StatusAndStatusChangeDetection sscd = PackedSinglePointWithSCD_getSCD(_io);
    return std::make_shared<StatusWithChangeDetection>(
        static_cast<FieldSet16>(
            (static_cast<uint16_t>(sscd->encodedValue[0]) << 0) +
            (static_cast<uint16_t>(sscd->encodedValue[1]) << 8)),
        static_cast<FieldSet16>(
            (static_cast<uint16_t>(sscd->encodedValue[2]) << 0) +
            (static_cast<uint16_t>(sscd->encodedValue[3]) << 8)),
        static_cast<Quality>(PackedSinglePointWithSCD_getQuality(_io)),
        std::nullopt, true);
  } break;
  case C_CS_NA_1: {
    // todo create class for Clock sync
    return std::make_shared<Generic>(
        COMMAND,
        Object::DateTime(ClockSynchronizationCommand_getTime(
            reinterpret_cast<ClockSynchronizationCommand>(io))),
        true);
  } break;
  case M_EI_NA_1:
    return std::make_shared<Generic>(MONITORING_EVENT, std::nullopt, true);
  case C_IC_NA_1:
  case C_CI_NA_1:
  case C_RD_NA_1:
  case C_TS_NA_1: {
    // allow get valid message to pass and extract informationObjectAddress
    return std::make_shared<Generic>(COMMAND, std::nullopt, true);
  } break;
  default:
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }
}

InformationObject Transformer::asInformationObject(
    std::shared_ptr<IInformation> info,
    const std::uint_fast32_t informationObjectAddress, const bool timestamp) {
  auto recorded_at = info->getRecordedAt();
  if (auto i = std::dynamic_pointer_cast<SingleInfo>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          SinglePointWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->isOn(),
              static_cast<uint8_t>(std::get<Quality>(i->getQuality())),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(SinglePointInformation_create(
        nullptr, informationObjectAddress, i->isOn(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  if (auto i = std::dynamic_pointer_cast<SingleCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          SingleCommandWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->isOn(), i->isSelect(),
              static_cast<uint8_t>(i->getQualifier()),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(SingleCommand_create(
        nullptr, informationObjectAddress, i->isOn(), i->isSelect(),
        static_cast<uint8_t>(i->getQualifier())));
  }
  if (auto i = std::dynamic_pointer_cast<DoubleInfo>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          DoublePointWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getState(),
              static_cast<uint8_t>(std::get<Quality>(i->getQuality())),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(DoublePointInformation_create(
        nullptr, informationObjectAddress, i->getState(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  if (auto i = std::dynamic_pointer_cast<DoubleCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          DoubleCommandWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getState(), i->isSelect(),
              static_cast<uint8_t>(i->getQualifier()),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(DoubleCommand_create(
        nullptr, informationObjectAddress, i->getState(), i->isSelect(),
        static_cast<uint8_t>(i->getQualifier())));
  }
  if (auto i = std::dynamic_pointer_cast<StepInfo>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          StepPositionWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getPosition().get(),
              i->isTransient(),
              static_cast<uint8_t>(std::get<Quality>(i->getQuality())),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(StepPositionInformation_create(
        nullptr, informationObjectAddress, i->getPosition().get(),
        i->isTransient(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  if (auto i = std::dynamic_pointer_cast<StepCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          StepCommandWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getStep(), i->isSelect(),
              static_cast<uint8_t>(i->getQualifier()),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(StepCommand_create(
        nullptr, informationObjectAddress, i->getStep(), i->isSelect(),
        static_cast<uint8_t>(i->getQualifier())));
  }
  if (auto i = std::dynamic_pointer_cast<NormalizedInfo>(info)) {
    // todo what about M_ME_ND_1: MeasuredValueNormalizedWithoutQuality_create(
    // nullptr, informationObjectAddress, i->getActual().get());
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          MeasuredValueNormalizedWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getActual().get(),
              static_cast<uint8_t>(std::get<Quality>(i->getQuality())),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(MeasuredValueNormalized_create(
        nullptr, informationObjectAddress, i->getActual().get(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  if (auto i = std::dynamic_pointer_cast<NormalizedCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          SetpointCommandNormalizedWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getTarget().get(),
              i->isSelect(), i->getQualifier().get(),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(SetpointCommandNormalized_create(
        nullptr, informationObjectAddress, i->getTarget().get(), i->isSelect(),
        i->getQualifier().get()));
  }
  if (auto i = std::dynamic_pointer_cast<ScaledInfo>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          MeasuredValueScaledWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getActual().get(),
              static_cast<uint8_t>(std::get<Quality>(i->getQuality())),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(MeasuredValueScaled_create(
        nullptr, informationObjectAddress, i->getActual().get(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  if (auto i = std::dynamic_pointer_cast<ScaledCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          SetpointCommandScaledWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getTarget().get(),
              i->isSelect(), i->getQualifier().get(),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(SetpointCommandScaled_create(
        nullptr, informationObjectAddress, i->getTarget().get(), i->isSelect(),
        i->getQualifier().get()));
  }
  if (auto i = std::dynamic_pointer_cast<ShortInfo>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          MeasuredValueShortWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getActual(),
              static_cast<uint8_t>(std::get<Quality>(i->getQuality())),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(MeasuredValueShort_create(
        nullptr, informationObjectAddress, i->getActual(),
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  if (auto i = std::dynamic_pointer_cast<ShortCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          SetpointCommandShortWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getTarget(), i->isSelect(),
              i->getQualifier().get(), recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(SetpointCommandShort_create(
        nullptr, informationObjectAddress, i->getTarget(), i->isSelect(),
        i->getQualifier().get()));
  }
  if (auto i = std::dynamic_pointer_cast<BinaryInfo>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          Bitstring32WithCP56Time2a_create(nullptr, informationObjectAddress,
                                           i->getBlob().get(),
                                           recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(BitString32_create(
        nullptr, informationObjectAddress, i->getBlob().get()));
  }
  if (auto i = std::dynamic_pointer_cast<BinaryCmd>(info)) {
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          Bitstring32CommandWithCP56Time2a_create(
              nullptr, informationObjectAddress, i->getBlob().get(),
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(Bitstring32Command_create(
        nullptr, informationObjectAddress, i->getBlob().get()));
  }
  if (auto i = std::dynamic_pointer_cast<BinaryCounterInfo>(info)) {
    auto q = std::get<BinaryCounterQuality>(i->getQuality());
    BinaryCounterReading _value = BinaryCounterReading_create(
        nullptr, i->getCounterFrozen(), i->getSequence().get(),
        ::test(q, BinaryCounterQuality::Carry),
        ::test(q, BinaryCounterQuality::Adjusted),
        ::test(q, BinaryCounterQuality::Invalid));
    if (timestamp) {
      return reinterpret_cast<InformationObject>(
          IntegratedTotalsWithCP56Time2a_create(
              nullptr, informationObjectAddress, _value,
              recorded_at.value().getEncoded()));
    }
    return reinterpret_cast<InformationObject>(
        IntegratedTotals_create(nullptr, informationObjectAddress, _value));
  }
  if (auto i = std::dynamic_pointer_cast<ProtectionEquipmentEventInfo>(info)) {
    if (!timestamp)
      throw std::invalid_argument("Only information with timestamp supported");
    sCP16Time2a elapsed{};
    CP16Time2a_setEplapsedTimeInMs(&elapsed, i->getElapsed_ms().get());
    tSingleEvent event =
        ((static_cast<uint8_t>(i->getState()) & 0b00000111) |
         (static_cast<uint8_t>(std::get<Quality>(i->getQuality())) &
          0b11111000));
    return reinterpret_cast<InformationObject>(
        EventOfProtectionEquipmentWithCP56Time2a_create(
            nullptr, informationObjectAddress, &event, &elapsed,
            recorded_at.value().getEncoded()));
  }
  if (auto i =
          std::dynamic_pointer_cast<ProtectionEquipmentStartEventsInfo>(info)) {
    if (!timestamp)
      throw std::invalid_argument("Only information with timestamp supported");
    sCP16Time2a elapsed{};
    CP16Time2a_setEplapsedTimeInMs(&elapsed, i->getRelayDuration_ms().get());
    return reinterpret_cast<InformationObject>(
        PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(
            nullptr, informationObjectAddress,
            static_cast<uint8_t>(i->getEvents()),
            static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &elapsed,
            recorded_at.value().getEncoded()));
  }
  if (auto i = std::dynamic_pointer_cast<ProtectionEquipmentOutputCircuitInfo>(
          info)) {
    if (!timestamp)
      throw std::invalid_argument("Only type with timestamp supported");
    sCP16Time2a elapsed{};
    CP16Time2a_setEplapsedTimeInMs(&elapsed, i->getRelayOperating_ms().get());
    return reinterpret_cast<InformationObject>(
        PackedOutputCircuitInfoWithCP56Time2a_create(
            nullptr, informationObjectAddress,
            static_cast<uint8_t>(i->getCircuits()),
            static_cast<uint8_t>(std::get<Quality>(i->getQuality())), &elapsed,
            recorded_at.value().getEncoded()));
  }
  if (auto i = std::dynamic_pointer_cast<StatusWithChangeDetection>(info)) {
    if (timestamp)
      throw std::invalid_argument("Only type without timestamp supported");
    sStatusAndStatusChangeDetection sscd{};
    auto status = static_cast<uint16_t>(i->getStatus());
    auto changed = static_cast<uint16_t>(i->getChanged());
    sscd.encodedValue[0] = (status >> 0) & 0b11111111;
    sscd.encodedValue[1] = (status >> 8) & 0b11111111;
    sscd.encodedValue[2] = (changed >> 0) & 0b11111111;
    sscd.encodedValue[3] = (changed >> 8) & 0b11111111;
    return reinterpret_cast<InformationObject>(PackedSinglePointWithSCD_create(
        nullptr, informationObjectAddress, &sscd,
        static_cast<uint8_t>(std::get<Quality>(i->getQuality()))));
  }
  throw std::runtime_error("Unknown information instance");
}
