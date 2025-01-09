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

#include <algorithm>
#include <atomic>
#include <bitset>
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
#include <sstream>
#include <string>
#include <thread>
#include <variant>
#include <vector>

#include "enums.h"
#include "numbers.h"

#define DEBUG_PRINT_CONDITION(X, Y, Z)                                         \
  ((X) ? printDebugMessage((Y), (Z)) : (void)0)
#define DEBUG_PRINT(mode, Y)                                                   \
  (::test(GLOBAL_DEBUG_MODE.load(), mode) ? printDebugMessage(mode, (Y))       \
                                          : (void)0)
#define DEBUG_TEST(mode) ::test(GLOBAL_DEBUG_MODE.load(), mode)

#define MICRO_SEC_STR u8" \xc2\xb5s"
#define DIFF_MS(begin, end)                                                    \
  std::chrono::duration_cast<std::chrono::microseconds>((end) - (begin)).count()
#define TICTOC(begin, end)                                                     \
  (std::to_string(DIFF_MS(begin, end)) +                                       \
   std::string(reinterpret_cast<const char *>(MICRO_SEC_STR)))
#define TICTOCNOW(begin) TICTOC(begin, std::chrono::steady_clock::now())
#define MAX_INFORMATION_OBJECT_ADDRESS 16777215
#define UNDEFINED_INFORMATION_OBJECT_ADDRESS 16777216

/**
 * @brief Global atomic variable storing the current debug mode configuration.
 *
 * This variable is used to manage and track the enabled debug settings across
 * the system. It is represented as a value from the Debug enum, supporting
 * bitwise operations for managing multiple debug modes simultaneously.
 *
 * The initial value is set to Debug::None, indicating that no debug mode
 * is enabled. This value can be modified using helper functions such as
 * setDebug, enableDebug, and disableDebug.
 *
 * Thread-safe operations on this variable ensure consistent behavior in
 * concurrent environments.
 */
extern std::atomic<Debug> GLOBAL_DEBUG_MODE;

/**
 * @brief Sets the global debug mode configuration.
 * @param mode The debug mode to be set, represented as a value of the Debug
 * enum.
 */
void setDebug(Debug mode);

/**
 * @brief Retrieves the current global debug mode configuration.
 * @return The current debug mode as a value of the Debug enum.
 */
Debug getDebug();

/**
 * @brief Enables the specified debug mode in the global debug configuration.
 * @param mode Debug mode to be enabled.
 */
void enableDebug(Debug mode);

/**
 * @brief Disables the specified debug mode in the global debug configuration.
 * @param mode Debug mode to be disabled.
 */
void disableDebug(Debug mode);

/**
 * @brief Prints a debug message if the specified debug mode is enabled.
 * @param mode Debug mode to check against the global debug mode.
 * @param message The message to be printed if the debug mode is enabled.
 */
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
 * @brief Converts a system_clock::time_point to a human-readable string format.
 * @param time A time_point object representing a point in time.
 * @return A string representation of the time in ISO 8601 format with
 * milliseconds and timezone offset.
 */
std::string
TimePoint_toString(const std::chrono::system_clock::time_point &time);

/**
 * @brief Converts an optional system_clock::time_point to a human-readable
 * string format.
 * @param time An optional time_point object representing a point in time.
 * @return A string representation of the time in ISO 8601 format with
 * milliseconds and timezone offset if the value is present, or "None" if the
 * optional is empty.
 */
std::string TimePoint_toString(
    const std::optional<std::chrono::system_clock::time_point> &time);

/**
 * @brief Converts a CP56Time2a timestamp to a
 * std::chrono::system_clock::time_point.
 * @param time The CP56Time2a timestamp to be converted.
 * @return A std::chrono::system_clock::time_point representing the given
 * CP56Time2a timestamp.
 */
std::chrono::system_clock::time_point to_time_point(CP56Time2a time);

/**
 * @brief Converts a system clock time point to a CP56Time2a timestamp.
 * @param time A pointer to the CP56Time2a structure where the converted time
 * will be stored.
 * @param time_point The std::chrono::system_clock::time_point representing the
 * input system time.
 */
void from_time_point(CP56Time2a time,
                     std::chrono::system_clock::time_point time_point);

/**
 * @brief Represents a task with a name, description, and completion status.
 *
 * Provides functionality to manage and query the task's state, including
 * marking it as complete.
 */
struct Task {
  std::function<void()> function;
  std::chrono::steady_clock::time_point schedule_time;
  bool operator<(const Task &rhs) const {
    return schedule_time > rhs.schedule_time;
  }
};

/**
 * @brief Defines the threshold duration for task delay warnings.
 * @details This constant is used to compare against the actual delay of a
 * scheduled task execution. If the delay exceeds this threshold,
 * a warning message will be logged indicating that the task execution
 * was delayed beyond the acceptable duration.
 */
constexpr auto TASK_DELAY_THRESHOLD = std::chrono::milliseconds(100);

// use primitives at the end, to avoid pybind11 down-casts (i .e. int)
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

// forward declaration to avoid .h loop inclusion
namespace Object {
class DateTime;
class DataPoint;
class Information;

class Station;
} // namespace Object

namespace Remote {
namespace Message {
class IncomingMessage;
class OutgoingMessage;
class Batch;
class PointMessage;
class PointCommand;
} // namespace Message
class Connection;
class TransportSecurity;
} // namespace Remote

class Server;

class Client;

namespace py = pybind11;

#endif // C104_TYPES_H
