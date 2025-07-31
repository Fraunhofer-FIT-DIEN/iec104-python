/**
 * Copyright 2020-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file debug.cpp
 * @brief debugging and logging utilities
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <iostream>
#include <numeric>
#include <sstream>
#include <vector>

#include "bitflag.h"
#include "debug.h"

std::atomic<Debug> GLOBAL_DEBUG_MODE{Debug::None};

void setDebug(Debug mode) { GLOBAL_DEBUG_MODE.store(mode); }

Debug getDebug() { return GLOBAL_DEBUG_MODE.load(); }

void enableDebug(Debug mode) {
  GLOBAL_DEBUG_MODE.store(GLOBAL_DEBUG_MODE.load() | mode);
}

void disableDebug(Debug mode) {
  GLOBAL_DEBUG_MODE.store(GLOBAL_DEBUG_MODE.load() & ~mode);
}

void printDebugMessage(const Debug context, const std::string &message) {
  std::ostringstream oss;
  oss << "[c104." << Debug_toFlagString(context) << "] " << message
      << std::endl;
  std::cout << oss.str();
  std::cout.flush();
}

void printDebugMessage(const std::string &context, const std::string &message) {
  std::ostringstream oss;
  oss << "[c104." << context << "] " << message << std::endl;
  std::cout << oss.str();
  std::cout.flush();
}

std::string Debug_toString(const Debug &mode) {
  if (is_none(mode)) {
    return "Debug set: {}, is_none: True";
  }
  std::vector<std::string> sv{};
  if (test(mode, Debug::Server))
    sv.emplace_back("Server");
  if (test(mode, Debug::Client))
    sv.emplace_back("Client");
  if (test(mode, Debug::Connection))
    sv.emplace_back("Connection");
  if (test(mode, Debug::Station))
    sv.emplace_back("Station");
  if (test(mode, Debug::Point))
    sv.emplace_back("Point");
  if (test(mode, Debug::Message))
    sv.emplace_back("Message");
  if (test(mode, Debug::Callback))
    sv.emplace_back("Callback");
  if (test(mode, Debug::Gil))
    sv.emplace_back("Gil");
  if (sv.empty())
    return "Debug set: { UNSUPPORTED_BITS_DETECTED }, is_none: False";
  return "Debug set: { " +
         std::accumulate(std::next(sv.begin()), sv.end(), sv[0],
                         [](const std::string &a, const std::string &b) {
                           return a + " | " + b;
                         }) +
         " }, is_none: False";
}

std::string Debug_toFlagString(const Debug &mode) {
  if (is_none(mode)) {
    return "None";
  }
  std::vector<std::string> sv{};
  if (test(mode, Debug::Server))
    sv.emplace_back("Server");
  if (test(mode, Debug::Client))
    sv.emplace_back("Client");
  if (test(mode, Debug::Connection))
    sv.emplace_back("Connection");
  if (test(mode, Debug::Station))
    sv.emplace_back("Station");
  if (test(mode, Debug::Point))
    sv.emplace_back("Point");
  if (test(mode, Debug::Message))
    sv.emplace_back("Message");
  if (test(mode, Debug::Callback))
    sv.emplace_back("Callback");
  if (test(mode, Debug::Gil))
    sv.emplace_back("Gil");
  if (sv.empty())
    return "UNSUPPORTED_BITS_DETECTED";
  return std::accumulate(
      std::next(sv.begin()), sv.end(), sv[0],
      [](const std::string &a, const std::string &b) { return a + " | " + b; });
}
