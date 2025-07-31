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
 * @file types.h
 * @brief collection of framework widely used data structures
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_TYPES_H
#define C104_TYPES_H

#include "cs101_information_objects.h"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>

class LimitedInt7;
class Byte32;
class NormalizedFloat;
class LimitedInt16;
enum class FieldSet16;
enum class Quality;
enum class BinaryCounterQuality;
enum class StartEvents;
enum class OutputCircuits;

// use primitives at the end to avoid pybind11 down-casts (i .e. int)
typedef std::variant<std::monostate, DoublePointValue, LimitedInt7,
                     StepCommandValue, Byte32, NormalizedFloat, LimitedInt16,
                     EventState, StartEvents, OutputCircuits, FieldSet16, bool,
                     float, int32_t>
    InfoValue;
typedef std::variant<std::monostate, Quality, BinaryCounterQuality> InfoQuality;

/**
 * @brief Converts an InfoValue object to its string representation.
 * @param value The InfoValue object to be converted.
 * @return A string representation of the provided InfoValue object.
 */
std::string InfoValue_toString(const InfoValue &value);

/**
 * @brief Converts an InfoQuality object to its string representation.
 * @param value The InfoQuality object to be converted.
 * @return A string representation of the given InfoQuality object.
 */
std::string InfoQuality_toString(const InfoQuality &value);

/**
 * @brief Convert a boolean value to its string representation.
 * @param val The boolean value to be converted.
 * @return A string representation of the boolean value ("True" or "False").
 */
std::string bool_toString(const bool &val);

/**
 * @brief Converts a Byte32 object to its binary string representation.
 * @param byte A reference to a Byte32 object to be converted.
 * @return A string containing the 32-bit binary representation of the Byte32
 * object prefixed with "0b".
 */
std::string Byte32_toString(const Byte32 &byte);

/**
 * @brief Validate and convert an ip address from string to in_addr struct
 * @param s ipv4 address in string representation
 * @return ipv4 address in in_addr struct format
 * @throws std::invalid_argument if conversion failed
 */
void Assert_IPv4(const std::string &s);

/**
 * @brief Validate a port number
 * @throws std::invalid_argument if port is < 1 or > 65535
 */
void Assert_Port(std::int_fast64_t port);

#endif // C104_TYPES_H
