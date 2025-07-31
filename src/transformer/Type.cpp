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

#include "transformer/Type.h"
#include "object/information/BinaryCmd.h"
#include "object/information/BinaryInfo.h"
#include "object/information/DoubleCmd.h"
#include "object/information/DoubleInfo.h"
#include "object/information/EquipmentProtectionEvent.h"
#include "object/information/EquipmentProtectionOutputCircuitEvent.h"
#include "object/information/EquipmentProtectionStartEvents.h"
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
Transformer::fromType(const IEC60870_5_TypeID type) {
  switch (type) {
  case M_SP_NA_1:
  case M_SP_TB_1:
    return std::make_shared<SingleInfo>(false, Quality::None, std::nullopt,
                                        false);
  case C_SC_NA_1:
  case C_SC_TA_1:
    return std::make_shared<SingleCmd>(
        false, false, CS101_QualifierOfCommand::NONE, std::nullopt, false);
  case M_DP_NA_1:
  case M_DP_TB_1:
    return std::make_shared<DoubleInfo>(IEC60870_DOUBLE_POINT_OFF,
                                        Quality::None, std::nullopt, false);
  case C_DC_NA_1:
  case C_DC_TA_1:
    return std::make_shared<DoubleCmd>(IEC60870_DOUBLE_POINT_OFF, false,
                                       CS101_QualifierOfCommand::NONE,
                                       std::nullopt, false);
  case M_ST_NA_1:
  case M_ST_TB_1:
    return std::make_shared<StepInfo>(LimitedInt7(0), false, Quality::None,
                                      std::nullopt, false);
  case C_RC_NA_1:
  case C_RC_TA_1:
    return std::make_shared<StepCmd>(IEC60870_STEP_LOWER, false,
                                     CS101_QualifierOfCommand::NONE,
                                     std::nullopt, false);
  case M_ME_NA_1:
  case M_ME_ND_1:
  case M_ME_TD_1:
    return std::make_shared<NormalizedInfo>(NormalizedFloat(0), Quality::None,
                                            std::nullopt, false);
  case C_SE_NA_1:
  case C_SE_TA_1:
    return std::make_shared<NormalizedCmd>(
        NormalizedFloat(0), false, LimitedUInt7(0), std::nullopt, false);
  case M_ME_NB_1:
  case M_ME_TE_1:
    return std::make_shared<ScaledInfo>(LimitedInt16(0), Quality::None,
                                        std::nullopt, false);
  case C_SE_NB_1:
  case C_SE_TB_1:
    return std::make_shared<ScaledCmd>(LimitedInt16(0), false, LimitedUInt7(0),
                                       std::nullopt, false);
  case M_ME_NC_1:
  case M_ME_TF_1:
    return std::make_shared<ShortInfo>(0, Quality::None, std::nullopt, false);
  case C_SE_NC_1:
  case C_SE_TC_1:
    return std::make_shared<ShortCmd>(0, false, LimitedUInt7(0), std::nullopt,
                                      false);
  case M_BO_NA_1:
  case M_BO_TB_1:
    return std::make_shared<BinaryInfo>(Byte32(0), Quality::None, std::nullopt,
                                        false);
  case C_BO_NA_1:
  case C_BO_TA_1:
    return std::make_shared<BinaryCmd>(Byte32(0), std::nullopt, false);
  case M_IT_NA_1:
  case M_IT_TB_1:
    return std::make_shared<BinaryCounterInfo>(
        0, LimitedUInt5(0), BinaryCounterQuality::None, std::nullopt, false);
  case M_EP_TD_1:
    return std::make_shared<ProtectionEquipmentEventInfo>(
        IEC60870_EVENTSTATE_OFF, LimitedUInt16(0), Quality::None, std::nullopt,
        false);
  case M_EP_TE_1:
    return std::make_shared<ProtectionEquipmentStartEventsInfo>(
        StartEvents::None, LimitedUInt16(0), Quality::None, std::nullopt,
        false);
  case M_EP_TF_1:
    return std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        OutputCircuits::None, LimitedUInt16(0), Quality::None, std::nullopt,
        false);
  case M_PS_NA_1:
    return std::make_shared<StatusWithChangeDetection>(
        static_cast<FieldSet16>(0), static_cast<FieldSet16>(0), Quality::None,
        std::nullopt, false);
  default:
    throw std::invalid_argument("Unsupported type " +
                                std::string(TypeID_toString(type)));
  }
}

IEC60870_5_TypeID Transformer::asType(std::shared_ptr<IInformation> info,
                                      const bool timestamp) {
  if (auto p = std::dynamic_pointer_cast<SingleInfo>(info)) {
    return timestamp ? M_SP_TB_1 : M_SP_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<SingleCmd>(info)) {
    return timestamp ? C_SC_TA_1 : C_SC_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<DoubleInfo>(info)) {
    return timestamp ? M_DP_TB_1 : M_DP_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<DoubleCmd>(info)) {
    return timestamp ? C_DC_TA_1 : C_DC_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<StepInfo>(info)) {
    return timestamp ? M_ST_TB_1 : M_ST_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<StepCmd>(info)) {
    return timestamp ? C_RC_TA_1 : C_RC_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<NormalizedInfo>(info)) {
    return timestamp ? M_ME_TD_1 : M_ME_NA_1; // todo what about M_ME_ND_1
  }
  if (auto p = std::dynamic_pointer_cast<NormalizedCmd>(info)) {
    return timestamp ? C_SE_TA_1 : C_SE_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<ScaledInfo>(info)) {
    return timestamp ? M_ME_TE_1 : M_ME_NB_1;
  }
  if (auto p = std::dynamic_pointer_cast<ScaledCmd>(info)) {
    return timestamp ? C_SE_TB_1 : C_SE_NB_1;
  }
  if (auto p = std::dynamic_pointer_cast<ShortInfo>(info)) {
    return timestamp ? M_ME_TF_1 : M_ME_NC_1;
  }
  if (auto p = std::dynamic_pointer_cast<ShortCmd>(info)) {
    return timestamp ? C_SE_TC_1 : C_SE_NC_1;
  }
  if (auto p = std::dynamic_pointer_cast<BinaryInfo>(info)) {
    return timestamp ? M_BO_TB_1 : M_BO_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<BinaryCmd>(info)) {
    return timestamp ? C_BO_TA_1 : C_BO_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<BinaryCounterInfo>(info)) {
    return timestamp ? M_IT_TB_1 : M_IT_NA_1;
  }
  if (auto p = std::dynamic_pointer_cast<ProtectionEquipmentEventInfo>(info)) {
    if (!timestamp)
      throw std::invalid_argument("Only type with timestamp supported");
    return M_EP_TD_1;
  }
  if (auto p =
          std::dynamic_pointer_cast<ProtectionEquipmentStartEventsInfo>(info)) {
    if (!timestamp)
      throw std::invalid_argument("Only type with timestamp supported");
    return M_EP_TE_1;
  }
  if (auto p = std::dynamic_pointer_cast<ProtectionEquipmentOutputCircuitInfo>(
          info)) {
    if (!timestamp)
      throw std::invalid_argument("Only type with timestamp supported");
    return M_EP_TF_1;
  }
  if (auto p = std::dynamic_pointer_cast<StatusWithChangeDetection>(info)) {
    if (timestamp)
      throw std::invalid_argument("Only type without timestamp supported");
    return M_PS_NA_1;
  }
  throw std::runtime_error("Unknown Derived type");
}
