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
#include "remote/message/Batch.h"
#include "types.h"

TEST_CASE("Create batch", "[remote::message::batch]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point1 = station->addPoint(11, IEC60870_5_TypeID::M_ME_NC_1);
  auto point2 = station->addPoint(12, IEC60870_5_TypeID::M_ME_NC_1);
  auto point3 = station->addPoint(13, IEC60870_5_TypeID::M_ME_NC_1);
  auto point4 = station->addPoint(14, IEC60870_5_TypeID::M_ME_NC_1);
  auto point5 = station->addPoint(15, IEC60870_5_TypeID::M_ME_NC_1);

  auto last = point1->getProcessedAt();
  REQUIRE(last > std::chrono::system_clock::time_point::min());

  auto batch = Remote::Message::Batch::create(
      CS101_CauseOfTransmission::CS101_COT_SPONTANEOUS);
  REQUIRE(batch->getCauseOfTransmission() ==
          CS101_CauseOfTransmission::CS101_COT_SPONTANEOUS);
  REQUIRE(batch->getNumberOfObjects() == 0);
  REQUIRE(batch->getType() == IEC60870_5_TypeID::C_TS_TA_1); // default
  batch->addPoint(point1);
  REQUIRE(batch->getNumberOfObjects() == 1);
  REQUIRE(batch->getType() == IEC60870_5_TypeID::M_ME_NC_1);
  batch->addPoint(point2);
  REQUIRE(batch->getNumberOfObjects() == 2);
}
