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
 * @file DoubleInfo.cpp
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/information/DoubleInfo.h"
#include "variadic.h"
#include <sstream>

using namespace Object::Information;

std::shared_ptr<DoubleInfo>
DoubleInfo::create(const std::variant<DoublePointValue, int32_t> state,
                   const Quality quality,
                   const std::optional<DateTime> &recorded_at) {
  const auto visitor = overloaded{
      [](DoublePointValue v) { return v; },
      [](int32_t v) {
        if (v < 0 || v > 3)
          throw std::out_of_range("Not a valid c104.Double value (0-3)");
        return static_cast<DoublePointValue>(v);
      }};
  return std::make_shared<DoubleInfo>(std::visit(visitor, state), quality,
                                      recorded_at, false);
}

InfoValue DoubleInfo::getValueImpl() const { return state; }

void DoubleInfo::setValueImpl(const InfoValue val) {
  state = std::get<DoublePointValue>(val);
}

InfoQuality DoubleInfo::getQualityImpl() const { return quality; }

void DoubleInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string DoubleInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " state=" << DoublePointValue_toString(state)
      << ", quality=" << Quality_toString(quality) << ", "
      << IInformation::base_toString() << ">";
  return oss.str();
}
