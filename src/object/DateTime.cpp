/**
 * Copyright 2024-2024 Fraunhofer Institute for Applied Information Technology
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
 * @file DateTime.cpp
 * @brief date time with extra flags
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "DateTime.h"
#include "types.h"

DateTime::DateTime(const std::chrono::system_clock::time_point t =
                       std::chrono::system_clock::now())
    : time(t), substituted(false), invalid(false), summertime(false) {}

DateTime::DateTime(CP56Time2a t) {
  time = to_time_point(t);
  invalid = CP56Time2a_isInvalid(t);
  substituted = CP56Time2a_isSubstituted(t);
  summertime = CP56Time2a_isSummerTime(t);
}
