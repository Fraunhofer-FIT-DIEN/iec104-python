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
 * @file debug.h
 * @brief debugging and logging utilities
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_DEBUG_H
#define C104_DEBUG_H

#include <atomic>
#include <cstdint>
#include <string>

#include "bitflag.h"

#define DEBUG_TEST(mode) ::any_of(GLOBAL_DEBUG_MODE.load(), mode)
#define DEBUG_PRINT_CONDITION(X, Y, Z)                                         \
  ((X) ? printDebugMessage((Y), (Z)) : (void)0)
#define DEBUG_PRINT_NAMED(X, Y, Z)                                             \
  (DEBUG_TEST(X) ? printDebugMessage(Y, (Z)) : (void)0)
#define DEBUG_PRINT(X, Y) (DEBUG_TEST(X) ? printDebugMessage(X, (Y)) : (void)0)

#define MICRO_SEC_STR u8" \xc2\xb5s"
#define DIFF_MS(begin, end)                                                    \
  std::chrono::duration_cast<std::chrono::microseconds>((end) - (begin)).count()
#define TICTOC(begin, end)                                                     \
  (std::to_string(DIFF_MS(begin, end)) +                                       \
   std::string(reinterpret_cast<const char *>(MICRO_SEC_STR)))
#define TICTOCNOW(begin) TICTOC(begin, std::chrono::steady_clock::now())

/**
 * @brief Represents various debugging levels as enum_bitmask
 *
 * This enum class defines debugging categories that can be used to specify
 * and manage different areas of debugging in an application. Each category
 * is represented by a single bit and can be combined using bitwise operations.
 */
enum class Debug : std::uint8_t {
  None = 0,
  Server = 0x01,
  Client = 0x02,
  Connection = 0x04,
  Station = 0x08,
  Point = 0x10,
  Message = 0x20,
  Callback = 0x40,
  Gil = 0x80,
  All = 0xFF
};
constexpr bool enum_bitmask(Debug &&);
std::string Debug_toString(const Debug &mode);
std::string Debug_toFlagString(const Debug &mode);

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
 * @param context Debug context for printed message.
 * @param message The message to be printed if the debug mode is enabled.
 */
void printDebugMessage(Debug context, const std::string &message);
void printDebugMessage(const std::string &context, const std::string &message);

#endif // C104_DEBUG_H
