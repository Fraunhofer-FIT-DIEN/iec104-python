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
 * @file DateTime.h
 * @brief date time with extra flags
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_DATETIME_H
#define C104_OBJECT_DATETIME_H

#include <atomic>
#include <chrono>
#include <iec60870_common.h>
#include <pybind11/pybind11.h>
#include <string>

namespace py = pybind11;
using namespace std::chrono_literals;

namespace Object {
class Station;

class DateTime {
public:
  static DateTime now();
  static DateTime now(const std::shared_ptr<Station> &station,
                      bool readonly = false);

  DateTime(const DateTime &other);
  explicit DateTime(const py::object &py_datetime, bool isSubstituted = false,
                    bool isInvalid = false, bool isDaylightSavingTime = false);

  /**
   * Constructs a DateTime object with a specified or default time point.
   *
   * @param t The time point to initialize the DateTime object.
   *          If not provided, the current system clock time is used as the
   * default.
   * @return A DateTime object initialized with the specified or default time
   * point.
   */
  explicit DateTime(std::chrono::system_clock::time_point t);

  explicit DateTime(CP56Time2a t);

  ~DateTime() = default;

  [[nodiscard]] std::chrono::system_clock::time_point getTime() const {
    return time;
  }

  [[nodiscard]] bool isSubstituted() const;

  void setSubstituted(bool enabled);

  [[nodiscard]] bool isInvalid() const;

  void setInvalid(bool enabled);

  [[nodiscard]] bool isDaylightSavingTime() const;

  /**
   * @brief getter for daylight_saving_time (aka summertime)
   *
   * Setting this property will add one hour on top of timeZoneOffset
   *
   * @return indicate if this timestamp was recorded during daylight_saving_time
   */
  void setDaylightSavingTime(bool enabled);

  [[nodiscard]] std::chrono::seconds getTimeZoneOffset() const;

  void injectTimeZone(std::chrono::seconds offset, bool isDaylightSavingTime,
                      bool overrideDST = true);

  void convertTimeZone(std::chrono::seconds offset, bool isDaylightSavingTime);

  void setReadonly();
  [[nodiscard]] bool isReadonly() const { return readonly; }

  CP56Time2a getEncoded();

  DateTime &operator=(const DateTime &other);

  [[nodiscard]] py::object toPyDateTime() const;

  [[nodiscard]] std::string toString() const;

protected:
  /// @brief naive timestamp (timezone unaware)
  std::atomic<std::chrono::system_clock::time_point> time;

  /// @brief timezone offset in seconds that point timestamps are recorded in
  std::atomic<std::chrono::seconds> timeZoneOffset{0s};

  /// @brief timezone injection can only be used once
  std::atomic_bool timeZoneSet{false};

  /// @brief encoded timestamp structure
  sCP56Time2a cp56{0, 0, 0, 0, 0, 0, 0};
  ;
  std::mutex cp56_mutex;

  /// @brief flag timestamp as substituted (not reported by original information
  /// source)
  std::atomic_bool substituted{false};

  /// @brief flag timestamp as invalid
  std::atomic_bool invalid{false};

  /**
   * @brief Indicates whether the timestamp was recorded during daylight savings
   * time (summer time).
   *
   * The use of the summer time (SU) flag is optional but generally discouraged
   * - use UTC instead. A timestamp with the SU flag set represents the
   * identical time value as a timestamp with the SU flag unset, but with the
   * displayed value shifted exactly one hour earlier. This may help in
   * assigning the correct hour to information objects generated during the
   * first hour after transitioning from daylight savings time (summer time) to
   * standard time.
   */
  std::atomic_bool daylightSavingTime{false};

  /// @brief toggle, if modification is allowed or not
  std::atomic_bool readonly{false};

  /*

   month
   get: >encodedValue[5] & 0x0f
   set: encodedValue[5] = (uint8_t) ((encodedValue[5] & 0xf0) + (value & 0x0f))



   encodedValue[0] + (encodedValue[1] << 8)
   -> SECOND in ms:

   encodedValue[2]
   -> MINUTE lower 6 bits
   -> Bit(6) = Substituted-Flag
   -> Bit(7) = Invalid-Flag

   encodedValue[3]
   -> HOUR lower 5 bits
   -> Bit(5) = ???
   -> Bit(6) = ???
   -> Bit(7) = SummerTime-Flag

   encodedValue[4]
   -> DAY-of-Month lower 5 bits
   -> Day-of-Week upper 3 bits (0=unused, 1=monday ... 7=sunday)

   encodedValue[5]
   -> MONTH lower 4 bits
   -> ??? upper 4 bits

   encodedValue[6]
   -> YEAR lower 7 bits
   -> Bit(7) = ???

   century?te

   */
};

};     // namespace Object
#endif // C104_OBJECT_DATETIME_H
