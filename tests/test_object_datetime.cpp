/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied Information Technology
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
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

TEST_CASE("Create datetime", "[object::datetime]") {
  auto station = Object::Station::create(14, nullptr, nullptr);
  station->setTimeZoneOffset(7200s);
  station->setDaylightSavingTime(true);

  // test now constructor
  {
    const auto now = std::chrono::system_clock::now();
    auto dt1 = Object::DateTime::now();

    REQUIRE((dt1.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt1.getTimeZoneOffset() == 0s);
    REQUIRE(dt1.isDaylightSavingTime() == false);
    REQUIRE(dt1.isReadonly() == false);
  }

  // test now constructor with timeZone
  {
    const auto now = std::chrono::system_clock::now();
    auto dt2 = Object::DateTime::now(station, true);

    REQUIRE((dt2.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt2.getTimeZoneOffset() == 7200s);
    REQUIRE(dt2.isDaylightSavingTime() == true);
    REQUIRE(dt2.isReadonly() == true);
  }
}

TEST_CASE("Inject timezone into datetime", "[object::datetime]") {
  auto station = Object::Station::create(14, nullptr, nullptr);
  station->setTimeZoneOffset(7200s);
  station->setDaylightSavingTime(true);

  // test inject timezone offest
  {
    const auto now = std::chrono::system_clock::now();
    auto dt1 = Object::DateTime::now();
    dt1.injectTimeZone(7200s, false, false);
    REQUIRE((dt1.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt1.getTimeZoneOffset() == 7200s);
    REQUIRE(dt1.isDaylightSavingTime() == false);
  }

  // DST causes a change of the time zone
  {
    const auto now = std::chrono::system_clock::now();
    auto dt2 = Object::DateTime::now();
    dt2.injectTimeZone(7200s, true, false);
    REQUIRE((dt2.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt2.getTimeZoneOffset() == 3600s);
    REQUIRE(dt2.isDaylightSavingTime() == true);
  }

  // DST override does not cause a change of the time zone
  {
    const auto now = std::chrono::system_clock::now();
    auto dt3 = Object::DateTime::now();
    dt3.injectTimeZone(7200s, true, true);
    REQUIRE((dt3.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt3.getTimeZoneOffset() == 7200s);
    REQUIRE(dt3.isDaylightSavingTime() == true);
  }

  // cannot inject twice
  {
    const auto now = std::chrono::system_clock::now();
    auto dt4 = Object::DateTime::now(station, true);
    REQUIRE_THROWS_AS(dt4.injectTimeZone(7200s, true, true), std::logic_error);
  }
}

TEST_CASE("Convert timezone of datetime", "[object::datetime]") {
  auto station = Object::Station::create(14, nullptr, nullptr);
  station->setTimeZoneOffset(7200s);
  station->setDaylightSavingTime(true);

  const auto suDiff = 3600s;

  // convert from non-SU to SU
  {
    const auto now = std::chrono::system_clock::now();
    auto dt1 = Object::DateTime::now();
    const auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());

    dt1.injectTimeZone(7200s, false, false);
    REQUIRE((dt1.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt1.getTimeZoneOffset() == 7200s);
    REQUIRE(dt1.isDaylightSavingTime() == false);
    dt1.convertTimeZone(7200s, true);
    const auto diff1 = std::chrono::duration_cast<std::chrono::seconds>(
                           dt1.getTime().time_since_epoch()) -
                       suDiff;
    REQUIRE((diff1 - now_epoch) < std::chrono::microseconds(1));
    REQUIRE(dt1.getTimeZoneOffset() == 7200s);
    REQUIRE(dt1.isDaylightSavingTime() == true);
  }

  // do not change anything
  {
    const auto now = std::chrono::system_clock::now();
    auto dt2 = Object::DateTime::now();
    const auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());

    dt2.injectTimeZone(7200s, true, true);
    REQUIRE((dt2.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt2.getTimeZoneOffset() == 7200s);
    REQUIRE(dt2.isDaylightSavingTime() == true);
    dt2.convertTimeZone(7200s, true);
    const auto diff2 = std::chrono::duration_cast<std::chrono::seconds>(
        dt2.getTime().time_since_epoch());
    REQUIRE((diff2 - now_epoch) < std::chrono::microseconds(1));
    REQUIRE(dt2.getTimeZoneOffset() == 7200s);
    REQUIRE(dt2.isDaylightSavingTime() == true);
  }

  // convert from SU to non-SU
  {
    const auto now = std::chrono::system_clock::now();
    auto dt3 = Object::DateTime::now();
    const auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());

    dt3.injectTimeZone(7200s, true, true);
    REQUIRE((dt3.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt3.getTimeZoneOffset() == 7200s);
    REQUIRE(dt3.isDaylightSavingTime() == true);
    const auto p1 = std::chrono::duration_cast<std::chrono::seconds>(
        dt3.getTime().time_since_epoch());
    dt3.convertTimeZone(7200s, false);
    const auto p2 = std::chrono::duration_cast<std::chrono::seconds>(
        dt3.getTime().time_since_epoch());
    const auto diff3 = std::chrono::duration_cast<std::chrono::seconds>(
                           dt3.getTime().time_since_epoch()) +
                       suDiff;
    REQUIRE((diff3 - now_epoch) < std::chrono::microseconds(1));
    REQUIRE(dt3.getTimeZoneOffset() == 7200s);
    REQUIRE(dt3.isDaylightSavingTime() == false);
  }

  // convert from SU to non-SU
  {
    const auto now = std::chrono::system_clock::now();
    auto dt4 = Object::DateTime::now(station, true);
    const auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());

    REQUIRE((dt4.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt4.getTimeZoneOffset() == 7200s);
    REQUIRE(dt4.isDaylightSavingTime() == true);
    dt4.convertTimeZone(7200s, false);
    const auto diff4 = std::chrono::duration_cast<std::chrono::seconds>(
                           dt4.getTime().time_since_epoch()) +
                       suDiff;
    REQUIRE((diff4 - now_epoch) < std::chrono::microseconds(1));
    REQUIRE(dt4.getTimeZoneOffset() == 7200s);
    REQUIRE(dt4.isDaylightSavingTime() == false);
  }

  // reduce offset
  {
    const auto now = std::chrono::system_clock::now();
    auto dt5 = Object::DateTime::now(station, true);
    const auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());

    REQUIRE((dt5.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt5.getTimeZoneOffset() == 7200s);
    REQUIRE(dt5.isDaylightSavingTime() == true);
    dt5.convertTimeZone(0s, true);
    const auto diff5 = std::chrono::duration_cast<std::chrono::seconds>(
                           dt5.getTime().time_since_epoch()) -
                       7200s;
    REQUIRE((diff5 - now_epoch) < std::chrono::microseconds(1));
    REQUIRE(dt5.getTimeZoneOffset() == 0s);
    REQUIRE(dt5.isDaylightSavingTime() == true);
  }

  // reduce offest
  {
    const auto now = std::chrono::system_clock::now();
    auto dt6 = Object::DateTime::now();
    const auto now_epoch = std::chrono::duration_cast<std::chrono::seconds>(
        now.time_since_epoch());

    REQUIRE((dt6.getTime().time_since_epoch() - now.time_since_epoch()) <
            std::chrono::microseconds(1));
    REQUIRE(dt6.getTimeZoneOffset() == 0s);
    REQUIRE(dt6.isDaylightSavingTime() == false);
    dt6.convertTimeZone(3600s, false);
    const auto diff6 = std::chrono::duration_cast<std::chrono::seconds>(
                           dt6.getTime().time_since_epoch()) -
                       3600s;
    REQUIRE((diff6 - now_epoch) < std::chrono::microseconds(1));
    REQUIRE(dt6.getTimeZoneOffset() == 3600s);
    REQUIRE(dt6.isDaylightSavingTime() == false);
  }
}
