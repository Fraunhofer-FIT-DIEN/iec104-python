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
 * @file types.h
 * @brief collection of framework wide used data structures
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_TYPES_H
#define C104_TYPES_H

#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <pybind11/pybind11.h>
#include <queue>
#include <regex>
#include <string>
#include <thread>
#include <vector>

#include "enums.h"

#define DEBUG_PRINT_CONDITION(X, Y, Z) (X ? printDebugMessage(Y, Z) : (void)0)
#define DEBUG_PRINT(mode, Y)                                                   \
  (::test(GLOBAL_DEBUG_MODE.load(), mode) ? printDebugMessage(mode, Y)         \
                                          : (void)0)
#define DEBUG_TEST(mode) ::test(GLOBAL_DEBUG_MODE.load(), mode)

extern std::atomic<Debug> GLOBAL_DEBUG_MODE;

void setDebug(Debug mode);

Debug getDebug();

void enableDebug(Debug mode);

void disableDebug(Debug mode);

void printDebugMessage(Debug mode, const std::string &message);

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
void Assert_Port(int_fast64_t port);

/**
 * @brief Get the time since epoch in milliseconds
 * @return number of milliseconds since 2000-01-01 00:00:00
 */
uint_fast64_t GetTimestamp_ms();

// forward declaration to avoid .h loop inclusion
namespace Object {
class DataPoint;

class Station;
} // namespace Object

namespace Remote {
namespace Message {
class IncomingMessage;

class OutgoingMessage;
} // namespace Message
class Connection;
class TransportSecurity;
} // namespace Remote

class Server;

class Client;

namespace py = pybind11;

#endif // C104_TYPES_H
