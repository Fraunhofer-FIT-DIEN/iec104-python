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
 * @file DateTime.cpp
 * @brief date time with extra flags
 *
 * @package iec104-python
 * @namespace Object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "DateTime.h"
#include "types.h"
#include <pybind11/chrono.h>

#include "Station.h"
#include "module/ScopedGilAcquire.h"

using namespace Object;

DateTime DateTime::now() { return DateTime(std::chrono::system_clock::now()); }

DateTime DateTime::now(const std::shared_ptr<Station> &station,
                       const bool readonly) {
  auto dt = now();
  if (station) {
    dt.injectTimeZone(station->getTimeZoneOffset(),
                      station->isDaylightSavingTime(), true);
    dt.setSubstituted(station->isAutoTimeSubstituted());
  }
  if (readonly) {
    dt.setReadonly();
  }
  return dt;
}

DateTime::DateTime(const DateTime &other) {
  time = other.time.load();
  timeZoneOffset = other.timeZoneOffset.load();
  substituted = other.substituted.load();
  invalid = other.invalid.load();
  daylightSavingTime = other.daylightSavingTime.load();
}

DateTime::DateTime(const py::object &py_datetime, const bool isSubstituted,
                   const bool isInvalid, const bool isDaylightSavingTime)
    : substituted(isSubstituted), invalid(isInvalid),
      daylightSavingTime(isDaylightSavingTime) {
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

  // tzinfo is present: store the offset in seconds
  const auto utcoffset = py_datetime.attr("utcoffset")();
  if (!utcoffset.is_none()) {
    timeZoneOffset.store(utcoffset.cast<std::chrono::seconds>());
  }
}

DateTime::DateTime(const std::chrono::system_clock::time_point t) : time(t) {}

DateTime::DateTime(const CP56Time2a t) {
  time = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(CP56Time2a_toMsTimestamp(t)));
  invalid = CP56Time2a_isInvalid(t);
  substituted = CP56Time2a_isSubstituted(t);
  daylightSavingTime = CP56Time2a_isSummerTime(t);
}

// Copy Assignment Operator
DateTime &DateTime::operator=(const DateTime &other) {
  if (this != &other) {
    // Protect against self-assignment
    if (readonly) {
      throw std::logic_error("DateTime is read-only!");
    }
    time = other.time.load();
    timeZoneOffset = other.timeZoneOffset.load();
    substituted = other.substituted.load();
    invalid = other.invalid.load();
    daylightSavingTime = other.daylightSavingTime.load();
  }
  return *this;
}

void DateTime::setReadonly() {
  if (readonly) {
    return;
  }
  readonly.store(true);
}

CP56Time2a DateTime::getEncoded() {
  const auto milliseconds =
      std::chrono::duration_cast<std::chrono::milliseconds>(
          time.load().time_since_epoch())
          .count();

  std::lock_guard<std::mutex> lock(cp56_mutex);
  CP56Time2a_setFromMsTimestamp(&cp56, milliseconds);
  CP56Time2a_setSubstituted(&cp56, substituted);
  CP56Time2a_setInvalid(&cp56, invalid);
  CP56Time2a_setSummerTime(&cp56, daylightSavingTime);

  return &cp56;
}

bool DateTime::isSubstituted() const { return substituted; }

void DateTime::setSubstituted(const bool enabled) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  substituted = enabled;
}

bool DateTime::isInvalid() const { return invalid; }

void DateTime::setInvalid(const bool enabled) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  invalid = enabled;
}

bool DateTime::isDaylightSavingTime() const { return daylightSavingTime; }

void DateTime::setDaylightSavingTime(const bool enabled) {
  if (readonly) {
    throw std::logic_error("DateTime is read-only!");
  }
  daylightSavingTime.store(enabled);
}

std::chrono::seconds DateTime::getTimeZoneOffset() const {
  return timeZoneOffset.load();
}

void DateTime::injectTimeZone(const std::chrono::seconds offset,
                              const bool isDaylightSavingTime,
                              const bool overrideDST) {
  // only once
  if (timeZoneSet.load()) {
    throw std::logic_error("DateTime is read-only!");
  }
  timeZoneSet.store(true);

  timeZoneOffset.store(offset);
  if (overrideDST) {
    daylightSavingTime.store(isDaylightSavingTime);
  } else {
    const bool old = daylightSavingTime.load();
    if (old != isDaylightSavingTime) {
      // message received with different SU flag than configured in station ->
      // correct timeZoneOffset
      const std::chrono::seconds modifier = old ? 3600s : -3600s;
      DEBUG_PRINT(Debug::Point, "DateTime.inject] Different SummerTime (DST) "
                                "flag in Info and Station | Station " +
                                    bool_toString(isDaylightSavingTime) +
                                    " | Info " +
                                    bool_toString(daylightSavingTime.load()) +
                                    " | timezone_offset modified by " +
                                    std::to_string(modifier.count()) + "s");
      daylightSavingTime.store(isDaylightSavingTime);
      timeZoneOffset.store(offset + modifier);
    }
  }
}

void DateTime::convertTimeZone(const std::chrono::seconds offset,
                               const bool isDaylightSavingTime) {
  // correct DST first to end with desired offset
  const bool old = daylightSavingTime.load();
  if (old != isDaylightSavingTime) {
    // message received with different SU flag than configured in station ->
    // correct timeZoneOffset
    const std::chrono::seconds modifier = old ? 3600s : -3600s;
    DEBUG_PRINT(Debug::Point, "DateTime.convert] Different SummerTime (DST) "
                              "flag in Info and Station | Station " +
                                  bool_toString(isDaylightSavingTime) +
                                  " | Info " +
                                  bool_toString(daylightSavingTime.load()) +
                                  " | timezone_offset modified by " +
                                  std::to_string(modifier.count()) + "s");
    daylightSavingTime.store(isDaylightSavingTime);
    timeZoneOffset.store(offset + modifier);
  }

  const auto modifier = offset - timeZoneOffset.load();
  DEBUG_PRINT_CONDITION(modifier != std::chrono::seconds(0), Debug::Point,
                        "DateTime.convert] Different TimeZoneOffset in Info "
                        "and Station | Station " +
                            std::to_string(offset.count()) + " | Info " +
                            std::to_string(timeZoneOffset.load().count()) +
                            " | timezone_offset modified by " +
                            std::to_string(modifier.count()) + "s");
  time.store(time.load() + modifier);
  timeZoneOffset.store(offset);
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

  const auto timestamp = static_cast<double>(milliseconds_since_epoch) / 1000.0;
  // Convert milliseconds to seconds as double

  // Prepare timezone offset
  const int timezone_offset_seconds =
      (timeZoneOffset.load() + (daylightSavingTime.load() ? 3600s : 0s))
          .count();

  const auto datetime = py::module::import("datetime");
  py::object tzinfo = py::none();

  // If there is a timezone offset specified, create a timezone using Python
  // datetime.timezone
  if (timezone_offset_seconds == 0) {
    tzinfo = datetime.attr("timezone").attr("utc");
  } else {
    tzinfo = datetime.attr("timezone")(
        datetime.attr("timedelta")(0, timezone_offset_seconds));
  }

  // Use fromtimestamp to create the datetime object with fractional seconds
  return datetime.attr("datetime")
      .attr("fromtimestamp")(py::float_(timestamp), tzinfo);
}

std::string DateTime::toString() const {
  std::ostringstream oss;

  using us_t = std::chrono::duration<int, std::micro>;
  auto us = std::chrono::duration_cast<us_t>(time.load().time_since_epoch() %
                                             std::chrono::seconds(1));
  if (us.count() < 0) {
    us += std::chrono::seconds(1);
  }

  // Adjust the time_point by the timezone offset
  auto offset =
      timeZoneOffset.load() + (daylightSavingTime.load() ? 3600s : 0s);
  const auto adjusted_time = time.load() + offset;

  const std::time_t tt = std::chrono::system_clock::to_time_t(
      std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          adjusted_time - us));

  const std::tm gm_tm = *std::gmtime(&tt);

  const int tz_hours = offset.count() / 3600;
  const int tz_minutes = (std::abs(offset.count()) % 3600) / 60;

  oss << "<c104.DateTime time=" << std::put_time(&gm_tm, "%Y-%m-%dT%H:%M:%S")
      << '.' << std::setw(3) << std::setfill('0') << (us.count() / 1000)
      << (tz_hours >= 0 ? "+" : "-") << std::setw(2) << std::setfill('0')
      << tz_hours << ":" << std::setw(2) << std::setfill('0') << tz_minutes

      << ", readonly=" << bool_toString(readonly)
      << ", invalid=" << bool_toString(invalid)
      << ", substituted=" << bool_toString(substituted)
      << ", daylight_saving_time=" << bool_toString(daylightSavingTime)
      << " at " << std::hex << std::showbase
      << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
}
