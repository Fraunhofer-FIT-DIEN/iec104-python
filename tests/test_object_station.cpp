/**
 * Copyright 2020-2026 Fraunhofer Institute for Applied Information Technology
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
#include "types.h"

TEST_CASE("Create station", "[object::station]") {
  auto station = Object::Station::create(14, nullptr, nullptr);
  REQUIRE(station->getCommonAddress() == 14);
}

// IOA 0 is a valid address and must behave like any other,
// otherwise addPoint(0) never dedups and a peer can grow `points` unbounded.
TEST_CASE("Station: IOA 0 is a regular, deduplicated address",
          "[object::station][security]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(1);

  auto p0 = station->addPoint(0, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(p0 != nullptr);
  REQUIRE(p0->getInformationObjectAddress() == 0);
  REQUIRE(station->getPoint(0).get() == p0.get());

  // repeated add at IOA 0 must not create a second point
  for (int i = 0; i < 50; ++i) {
    REQUIRE(station->addPoint(0, IEC60870_5_TypeID::M_SP_NA_1) == nullptr);
  }
  REQUIRE(station->getPoints().size() == 1);

  // remove / re-add round trip
  REQUIRE(station->removePoint(0) == true);
  REQUIRE(station->getPoint(0) == nullptr);
  REQUIRE(station->removePoint(0) == false);
  REQUIRE(station->hasPoints() == false);
  REQUIRE(station->addPoint(0, IEC60870_5_TypeID::M_ME_NC_1) != nullptr);
  REQUIRE(station->getPoint(0)->getType() == IEC60870_5_TypeID::M_ME_NC_1);
}

TEST_CASE("Station: getPoint lookup stays consistent with getPoints",
          "[object::station]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(2);
  constexpr std::uint_fast32_t N = 2000;

  for (std::uint_fast32_t ioa = 0; ioa < N; ++ioa) {
    REQUIRE(station->addPoint(ioa, IEC60870_5_TypeID::M_SP_NA_1) != nullptr);
  }
  REQUIRE(station->getPoints().size() == N);
  REQUIRE(station->getPoint(MAX_INFORMATION_OBJECT_ADDRESS) == nullptr);

  // every point is reachable by its own IOA and the result is the owned object
  for (auto const &p : station->getPoints()) {
    REQUIRE(station->getPoint(p->getInformationObjectAddress()).get() ==
            p.get());
  }

  // removing odd addresses leaves even ones reachable and odd ones gone
  for (std::uint_fast32_t ioa = 1; ioa < N; ioa += 2) {
    REQUIRE(station->removePoint(ioa) == true);
  }
  REQUIRE(station->getPoints().size() == N / 2);
  for (std::uint_fast32_t ioa = 0; ioa < N; ++ioa) {
    if (ioa % 2 == 0) {
      REQUIRE(station->getPoint(ioa) != nullptr);
    } else {
      REQUIRE(station->getPoint(ioa) == nullptr);
    }
  }

  // highest valid address is accepted as well
  auto top = station->addPoint(MAX_INFORMATION_OBJECT_ADDRESS,
                               IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(top != nullptr);
  REQUIRE(station->getPoint(MAX_INFORMATION_OBJECT_ADDRESS).get() == top.get());
}

// M8: an orphaned station (server/client gone) has no tick rate. addPoint
// must reject it, otherwise the point's report interval and timer checks
// would evaluate `% 0` (SIGFPE).
TEST_CASE("Orphaned station rejects addPoint", "[object::station][M8]") {
  auto standalone = Object::Station::create(3, nullptr, nullptr);
  REQUIRE_THROWS_AS(standalone->addPoint(1, IEC60870_5_TypeID::M_ME_NA_1, 1000),
                    std::runtime_error);
  REQUIRE_THROWS_AS(standalone->addPoint(2, IEC60870_5_TypeID::M_SP_NA_1),
                    std::runtime_error);
  REQUIRE_FALSE(standalone->hasPoints());

  // station outlives its server
  std::shared_ptr<Object::Station> station;
  {
    auto server = Server::create("127.0.0.1", 2404, 100);
    station = server->addStation(4);
    auto point = station->addPoint(1, IEC60870_5_TypeID::M_ME_NA_1, 1000);
    REQUIRE(point != nullptr);
    REQUIRE(point->getReportInterval_ms() == 1000);
  }
  REQUIRE(station->getServer() == nullptr);
  REQUIRE_THROWS_AS(station->addPoint(2, IEC60870_5_TypeID::M_ME_NA_1, 1000),
                    std::runtime_error);
  REQUIRE(station->getPoint(2) == nullptr);
}
