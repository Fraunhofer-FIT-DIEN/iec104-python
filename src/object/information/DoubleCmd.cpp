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
 * @file DoubleCmd.cpp
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/information/DoubleCmd.h"
#include "variadic.h"
#include <sstream>

using namespace Object::Information;

std::shared_ptr<DoubleCmd>
DoubleCmd::create(const std::variant<DoublePointValue, int32_t> state,
                  const CS101_QualifierOfCommand qualifier,
                  const std::optional<DateTime> &recorded_at) {
  const auto visitor = overloaded{
      [](DoublePointValue v) { return v; },
      [](int32_t v) {
        if (v < 0 || v > 3)
          throw std::out_of_range("Not a valid c104.Double value (0-3)");
        return static_cast<DoublePointValue>(v);
      }};
  return std::make_shared<DoubleCmd>(std::visit(visitor, state), false,
                                     qualifier, recorded_at, false);
}

InfoValue DoubleCmd::getValueImpl() const { return state; }

void DoubleCmd::setValueImpl(const InfoValue val) {
  state = std::get<DoublePointValue>(val);
}

std::string DoubleCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " state=" << DoublePointValue_toString(state)
      << ", qualifier=" << QualifierOfCommand_toString(qualifier) << ", "
      << IInformation::base_toString() << ">";
  return oss.str();
}
