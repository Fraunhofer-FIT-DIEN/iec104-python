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
 * @file NormalizedInfo.cpp
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/information/NormalizedInfo.h"
#include <sstream>

using namespace Object::Information;

InfoValue NormalizedInfo::getValueImpl() const { return actual; }

void NormalizedInfo::setValueImpl(const InfoValue val) {
  actual = std::get<NormalizedFloat>(val);
}

InfoQuality NormalizedInfo::getQualityImpl() const { return quality; }

void NormalizedInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string NormalizedInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " actual=" << std::to_string(actual.get())
      << ", quality=" << Quality_toString(quality) << ", "
      << IInformation::base_toString() << ">";
  return oss.str();
}
