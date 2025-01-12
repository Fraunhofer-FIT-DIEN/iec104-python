/**
 * Copyright 2024-2026 Fraunhofer Institute for Applied Information Technology
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
#include <pybind11/chrono.h>

#include "module/ScopedGilAcquire.h"

using namespace Object;

DateTime DateTime::now(const bool readonly) {
  // todo mark auto-created DateTimes as substituted?
  return DateTime(std::chrono::system_clock::now(), readonly);
}

DateTime::DateTime(const DateTime &other) {
  time = other.time.load();
  timezoneOffset = other.timezoneOffset.load();
  substituted = other.substituted.load();
  invalid = other.invalid.load();
  summertime = other.summertime.load();
  readonly = other.readonly.load();
}

DateTime::DateTime(const py::object &py_datetime, const bool readonly)
    : substituted(false), invalid(false), summertime(false),
      readonly(readonly) {
  Module::ScopedGilAcquire scoped("DateTime.fromPy");

  // Check whether it's a datetime.datetime object
  if (!py::hasattr(py_datetime, "timestamp")) {
    throw std::invalid_argument("Expected a datetime.datetime object");
  }

  // Retrieve the UNIX timestamp from the Python datetime object
  const auto timestamp =
      py_datetime.attr("timestamp")().cast<double>(); // Seconds since epoch

  // Convert timestamp to system_clock::time_point and initialize `time`
  const auto duration_since_epoch = std::chrono::duration<double>(timestamp);
  time = std::chrono::system_clock::time_point{
      std::chrono::duration_cast<std::chrono::system_clock::duration>(
          duration_since_epoch)};

  std::int64_t milliseconds = static_cast<std::int64_t>(timestamp * 1000);
  CP56Time2a_setFromMsTimestamp(&cp56, milliseconds);

  // tzinfo is present: store the offset in seconds
  auto utcoffset = py_datetime.attr("utcoffset")();
  if (!utcoffset.is_none()) {
    timezoneOffset = utcoffset.attr("total_seconds")().cast<int>();
  }
}

DateTime::DateTime(const std::chrono::system_clock::time_point t,
                   const bool readonly)
    : time(t), substituted(false), invalid(false), summertime(false),
      readonly(readonly) {}

DateTime::DateTime(const CP56Time2a t, const bool readonly)
    : readonly(readonly) {
  time = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(CP56Time2a_toMsTimestamp(t)));
  invalid = CP56Time2a_isInvalid(t);
  substituted = CP56Time2a_isSubstituted(t);
  summertime = CP56Time2a_isSummerTime(t);
}

// Copy Assignment Operator
DateTime &DateTime::operator=(const DateTime &other) {
  if (this != &other) { // Protect against self-assignment
    time = other.time.load();
    timezoneOffset = other.timezoneOffset.load();
    substituted = other.substituted.load();
    invalid = other.invalid.load();
    summertime = other.summertime.load();
    readonly = other.readonly.load();
  }
  return *this;
}

CP56Time2a DateTime::getEncoded() {
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          time.load().time_since_epoch())
          .count() +
      timezoneOffset * 1000;

  std::lock_guard<std::mutex> lock(cp56_mutex);
  CP56Time2a_setFromMsTimestamp(&cp56, milliseconds);
  CP56Time2a_setSubstituted(&cp56, substituted);
  CP56Time2a_setInvalid(&cp56, invalid);
  CP56Time2a_setSummerTime(&cp56, summertime);

  return &cp56;
}

bool DateTime::isSubstituted() const { return substituted; }

void DateTime::setSubstituted(const bool substituted) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  this->substituted = substituted;
}

bool DateTime::isInvalid() const { return invalid; }

void DateTime::setInvalid(const bool invalid) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  this->invalid = invalid;
}

bool DateTime::isSummertime() const { return summertime; }

void DateTime::setSummertime(const bool summertime) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  this->summertime = summertime;
}

std::int_fast16_t DateTime::getTimezoneOffset() const { return timezoneOffset; }

void DateTime::setTimezoneOffset(const std::int_fast16_t seconds) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  timezoneOffset = seconds;
}

py::object DateTime::toPyDateTime() const {
  Module::ScopedGilAcquire scoped("DateTime.toPy");

  PyDateTime_IMPORT;

  // pybind11/chrono.h caster code copy
  if (!PyDateTimeAPI) {
    PyDateTime_IMPORT;
  }

  const auto milliseconds_since_epoch =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          time.load().time_since_epoch())
          .count();

  const double timestamp = milliseconds_since_epoch /
                           1000.0; // Convert milliseconds to seconds as double

  // Prepare timezone offset in minutes
  int timezone_offset_seconds = timezoneOffset.load();
  py::object tzinfo = py::none();

  // If there is a timezone offset specified, create a timezone using Python
  // datetime.timezone
  if (timezone_offset_seconds != 0) {
    tzinfo = py::module::import("datetime")
                 .attr("timezone")(
                     py::module::import("datetime")
                         .attr("timedelta")(0, timezone_offset_seconds));
  }

  // Use fromtimestamp to create the datetime object with fractional seconds
  return py::module::import("datetime")
      .attr("datetime")
      .attr("fromtimestamp")(py::float_(timestamp), tzinfo);
}

std::string
TimePoint_toString(const std::chrono::system_clock::time_point &time) {
  using us_t = std::chrono::duration<int, std::micro>;
  auto us = std::chrono::duration_cast<us_t>(time.time_since_epoch() %
                                             std::chrono::seconds(1));
  if (us.count() < 0) {
    us += std::chrono::seconds(1);
  }

  std::time_t tt = std::chrono::system_clock::to_time_t(
      std::chrono::time_point_cast<std::chrono::system_clock::duration>(time -
                                                                        us));

  std::tm local_tt = *std::localtime(&tt);

  std::ostringstream oss;
  oss << std::put_time(&local_tt, "%Y-%m-%dT%H:%M:%S");
  oss << '.' << std::setw(3) << std::setfill('0') << us.count();
  oss << std::put_time(&local_tt, "%z");
  return oss.str();
}

std::string DateTime::toString() const {
  std::ostringstream oss;
  oss << "<c104.DateTime time=" << TimePoint_toString(time.load())
      << ", offset=" << std::to_string(timezoneOffset) << "min"
      << ", invalid=" << bool_toString(invalid)
      << ", substituted=" << bool_toString(substituted)
      << ", summertime=" << bool_toString(summertime) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
}
