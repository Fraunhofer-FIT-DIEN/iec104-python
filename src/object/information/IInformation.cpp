/**
 * Copyright 2024-2025 Fraunhofer Institute for Applied IInformation Technology
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
 * @file IInformation.cpp
 * @brief abstract protocol IInformation
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/information/IInformation.h"
#include "types.h"
#include "variadic.h"
#include <sstream>

using namespace Object::Information;

IInformation::IInformation(const std::optional<Object::DateTime> &recorded_at,
                           const bool readonly)
    : recorded_at(recorded_at), readonly(readonly),
      processed_at(std::chrono::system_clock::now()){};

InfoQuality IInformation::getQualityImpl() const { return std::monostate{}; }

void IInformation::setQualityImpl(const InfoQuality val){};

InformationCategory IInformation::getCategory() const {
  return MONITORING_STATUS;
}

const std::optional<Object::DateTime> &IInformation::getRecordedAt() const {
  return recorded_at;
}

const Object::DateTime &IInformation::getProcessedAt() const {
  return processed_at;
}

bool IInformation::isReadonly() const { return readonly; }

InfoValue IInformation::getValue() {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  return getValueImpl();
}

void IInformation::setValue(const InfoValue val) {
  if (readonly) {
    throw std::domain_error("IInformation is read-only!");
  }

  try {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    setValueImpl(val);
  } catch (const std::bad_variant_access &e) {
    throw std::invalid_argument(
        "Invalid value, please provide an instance of the matching "
        "IInformation object\n - Previous: " +
        InfoValue_toString(getValue()) +
        "\n - Proposed: " + InfoValue_toString(val));
  }
};

InfoQuality IInformation::getQuality() {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  return getQualityImpl();
}

void IInformation::setQuality(const InfoQuality val) {
  if (readonly) {
    throw std::domain_error("IInformation is read-only!");
  }

  try {
    std::lock_guard<std::recursive_mutex> lock(mtx);
    setQualityImpl(val);
  } catch (const std::bad_variant_access &e) {
    throw std::invalid_argument(
        "Invalid quality, please provide an instance of the matching "
        "IInformation object\n - Previous: " +
        InfoQuality_toString(getQuality()) +
        "\n - Proposed: " + InfoQuality_toString(val));
  }
};

void IInformation::setReadonly() {
  if (readonly) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(mtx);
  readonly = true;
};

void IInformation::setRecordedAt(const std::optional<Object::DateTime> val) {
  if (readonly) {
    return;
  }
  std::lock_guard<std::recursive_mutex> lock(mtx);
  recorded_at = val;
}

void IInformation::setProcessedAt(const Object::DateTime &val) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  processed_at = val;
}

void IInformation::injectTimeZone(const std::chrono::seconds offset,
                                  const bool daylightSavingTime) {
  std::lock_guard<std::recursive_mutex> lock(mtx);
  processed_at.injectTimeZone(offset, daylightSavingTime, true);
  if (recorded_at.has_value()) {
    recorded_at.value().injectTimeZone(offset, daylightSavingTime);
  }
}

std::string IInformation::base_toString() const {
  std::ostringstream oss;
  oss << "recorded_at="
      << (recorded_at.has_value() ? recorded_at.value().toString() : "None")
      << ", processed_at=" << processed_at.toString()
      << ", readonly=" << bool_toString(readonly) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this);
  return oss.str();
}

std::string IInformation::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " " << IInformation::base_toString() << ">";
  return oss.str();
}

struct InfoValueToStringVisitor {
  std::string operator()(const FieldSet16 &obj) {
    return FieldSet16_toString(obj);
  }
};

std::string InfoValue_toString(const InfoValue &value) {
  const auto visitor = overloaded{
      [](std::monostate v) { return std::string("N.A."); },
      [](DoublePointValue v) { return DoublePointValue_toString(v); },
      [](const LimitedInt7 &v) { return std::to_string(v.get()); },
      [](StepCommandValue v) { return StepCommandValue_toString(v); },
      [](const Byte32 &v) { return std::to_string(v.get()); },
      [](const NormalizedFloat &v) { return std::to_string(v.get()); },
      [](const LimitedInt16 &v) { return std::to_string(v.get()); },
      [](EventState v) { return EventState_toString(v); },
      [](const StartEvents &v) { return StartEvents_toString(v); },
      [](const OutputCircuits &v) { return OutputCircuits_toString(v); },
      [](const FieldSet16 &v) { return FieldSet16_toString(v); },
      [](bool v) { return bool_toString(v); },
      [](float v) { return std::to_string(v); },
      [](int32_t v) { return std::to_string(v); }};
  return std::visit(visitor, value);
}

std::string InfoQuality_toString(const InfoQuality &value) {
  const auto visitor =
      overloaded{[](std::monostate v) { return std::string("N.A."); },
                 [](const Quality &v) { return Quality_toString(v); },
                 [](const BinaryCounterQuality &v) {
                   return BinaryCounterQuality_toString(v);
                 }};
  return std::visit(visitor, value);
}
