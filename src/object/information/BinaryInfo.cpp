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
 * @file BinaryInfo.cpp
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/information/BinaryInfo.h"
#include "variadic.h"
#include <sstream>

using namespace Object::Information;

std::shared_ptr<BinaryInfo>
BinaryInfo::create(const std::variant<Byte32, py::bytes, int32_t> blob,
                   const Quality quality,
                   const std::optional<DateTime> &recorded_at) {
  const auto visitor = overloaded{[](Byte32 v) { return v; },
                                  [](py::bytes v) { return Byte32(v); },
                                  [](int32_t v) { return Byte32(v); }};
  return std::make_shared<BinaryInfo>(std::visit(visitor, blob), quality,
                                      recorded_at, false);
}

InfoValue BinaryInfo::getValueImpl() const { return blob; }

void BinaryInfo::setValueImpl(const InfoValue val) {
  blob = std::get<Byte32>(val);
}

InfoQuality BinaryInfo::getQualityImpl() const { return quality; }

void BinaryInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string BinaryInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " blob=" << Byte32_toString(blob)
      << ", quality=" << Quality_toString(quality) << ", "
      << IInformation::base_toString() << ">";
  return oss.str();
}
