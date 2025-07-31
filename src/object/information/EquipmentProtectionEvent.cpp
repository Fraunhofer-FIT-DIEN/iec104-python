/**
 * Copyright 2024-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file EquipmentProtectionEvent.cpp
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/information/EquipmentProtectionEvent.h"
#include "variadic.h"
#include <sstream>

using namespace Object::Information;

std::shared_ptr<ProtectionEquipmentEventInfo>
ProtectionEquipmentEventInfo::create(
    const std::variant<EventState, int32_t> state,
    const std::variant<LimitedUInt16, int32_t> &elapsed_ms,
    const Quality quality, const std::optional<DateTime> &recorded_at) {
  const auto visitor = overloaded{
      [](EventState v) { return v; },
      [](int32_t v) {
        if (v < 0 || v > 3)
          throw std::out_of_range("Not a valid c104.EventState value (0-3)");
        return static_cast<EventState>(v);
      }};
  const auto visitor2 = overloaded{[](LimitedUInt16 v) { return v; },
                                   [](int32_t v) { return LimitedUInt16(v); }};
  return std::make_shared<ProtectionEquipmentEventInfo>(
      std::visit(visitor, state), std::visit(visitor2, elapsed_ms), quality,
      recorded_at, false);
}

InfoValue ProtectionEquipmentEventInfo::getValueImpl() const { return state; }

void ProtectionEquipmentEventInfo::setValueImpl(const InfoValue val) {
  state = std::get<EventState>(val);
}

InfoQuality ProtectionEquipmentEventInfo::getQualityImpl() const {
  return quality;
}

void ProtectionEquipmentEventInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string ProtectionEquipmentEventInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " state=" << EventState_toString(state)
      << ", elapsed_ms=" << std::to_string(elapsed_ms.get())
      << ", quality=" << Quality_toString(quality) << ", "
      << IInformation::base_toString() << ">";
  return oss.str();
}
