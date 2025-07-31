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
 * @file types.cpp
 * @brief collection of framework widely used data structures
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "types.h"
#include "numbers.h"
#include <bitset>
#include <regex>
#include <stdexcept>

std::string bool_toString(const bool &val) { return val ? "True" : "False"; }

std::string Byte32_toString(const Byte32 &byte) {
  std::bitset<32> bits(byte.get());
  return "0b" + bits.to_string();
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
