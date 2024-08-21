/**
 * Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology
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
 * @file types.cpp
 * @brief collection of framework wide used data structures
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "types.h"
#include <sstream>

std::atomic<Debug> GLOBAL_DEBUG_MODE{Debug::None};

void setDebug(Debug mode) { GLOBAL_DEBUG_MODE.store(mode); }

Debug getDebug() { return GLOBAL_DEBUG_MODE.load(); }

void enableDebug(Debug mode) {
  GLOBAL_DEBUG_MODE.store(GLOBAL_DEBUG_MODE.load() | mode);
}

void disableDebug(Debug mode) {
  GLOBAL_DEBUG_MODE.store(GLOBAL_DEBUG_MODE.load() & ~mode);
}

void printDebugMessage(const Debug mode, const std::string &message) {
  if (test(GLOBAL_DEBUG_MODE.load(), mode)) {
    std::stringstream print_str{};
    print_str << "[c104." << Debug_toFlagString(mode) << "] " << message
              << std::endl;
    std::cout << print_str.str();
    std::cout.flush();
  }
}

void Assert_IPv4(const std::string &s) {
  if (s == "localhost" || s == "lo")
    return;

  std::regex const ipv4_regex("^(?:(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)\\.)"
                              "{3}(?:25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)$");
  if (!std::regex_search(s, ipv4_regex)) {
    throw std::invalid_argument("IP " + s + " is invalid!");
  }
}

void Assert_Port(const int_fast64_t port) {
  if (port < 1 || port > 65534)
    throw std::invalid_argument("Port " + std::to_string(port) +
                                " is invalid!");
}

std::chrono::system_clock::time_point to_time_point(const CP56Time2a time) {
  return std::chrono::system_clock::time_point(
      std::chrono::milliseconds(CP56Time2a_toMsTimestamp(time)));
}

void from_time_point(CP56Time2a time,
                     const std::chrono::system_clock::time_point time_point) {
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                    time_point.time_since_epoch())
                    .count();

  CP56Time2a_createFromMsTimestamp(time, static_cast<uint64_t>(millis));
}

struct InfoValueToStringVisitor {
  std::string operator()(std::monostate value) const { return "N.A."; }
  std::string operator()(bool value) const { return std::to_string(value); }
  std::string operator()(DoublePointValue value) const {
    return DoublePointValue_toString(value);
  }
  std::string operator()(const LimitedInt7 &obj) {
    return std::to_string(obj.get());
  }
  std::string operator()(StepCommandValue value) const {
    return StepCommandValue_toString(value);
  }
  std::string operator()(const Byte32 &obj) {
    return std::to_string(obj.get());
  }
  std::string operator()(const NormalizedFloat &obj) {
    return std::to_string(obj.get());
  }
  std::string operator()(const LimitedInt16 &obj) {
    return std::to_string(obj.get());
  }
  std::string operator()(float value) const { return std::to_string(value); }
  std::string operator()(int32_t value) const { return std::to_string(value); }
  std::string operator()(EventState value) const {
    return EventState_toString(value);
  }
  std::string operator()(const StartEvents &obj) {
    return StartEvents_toString(obj);
  }
  std::string operator()(const OutputCircuits &obj) {
    return OutputCircuits_toString(obj);
  }
  std::string operator()(const FieldSet16 &obj) {
    return FieldSet16_toString(obj);
  }
};

std::string InfoValue_toString(const InfoValue &value) {
  return std::visit(InfoValueToStringVisitor(), value);
}

struct QualityValueToStringVisitor {
  std::string operator()(std::monostate value) const { return "N. A."; }
  std::string operator()(const Quality &obj) { return Quality_toString(obj); }
  std::string operator()(const BinaryCounterQuality &obj) {
    return BinaryCounterQuality_toString(obj);
  }
};

std::string InfoQuality_toString(const InfoQuality &value) {
  return std::visit(QualityValueToStringVisitor(), value);
}
