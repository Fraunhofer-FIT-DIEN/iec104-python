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
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <catch2/catch_test_macros.hpp>

#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

TEST_CASE("Create point", "[object::point]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(point->getStation().get() == station.get());
  REQUIRE(point->getInformationObjectAddress() == 11);
  REQUIRE(point->getRelatedInformationObjectAddress() == 0);
  REQUIRE(point->getRelatedInformationObjectAutoReturn() == false);
  REQUIRE(point->getType() == IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(point->getReportInterval_ms() == 0);
  REQUIRE(std::get<Quality>(point->getInfo()->getQuality()) == Quality::None);
  REQUIRE(std::get<Quality>(point->getQuality()) == Quality::None);
  REQUIRE(std::get<bool>(point->getInfo()->getValue()) == false);
  REQUIRE(std::get<bool>(point->getValue()) == false);
  REQUIRE(point->getProcessedAt().getTime() >
          std::chrono::system_clock::time_point::min());
  REQUIRE(point->getRecordedAt().has_value() == false);
}

TEST_CASE("Set point value", "[object::point]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::M_ME_TE_1);

  point->setInfo(Object::ScaledInfo::create(
      LimitedInt16(334), Quality::Invalid,
      Object::DateTime(std::chrono::system_clock::time_point(
          std::chrono::milliseconds(1234567890)))));
  REQUIRE(std::get<LimitedInt16>(point->getValue()).get() ==
          LimitedInt16(334).get());
  REQUIRE(point->getRecordedAt().value().getTime() ==
          std::chrono::system_clock::time_point(
              std::chrono::milliseconds(1234567890)));
  REQUIRE(std::get<Quality>(point->getQuality()) == Quality::Invalid);
}

TEST_CASE("Set point value via message", "[object::point]") {
  auto server = Server::create();
  auto station = server->addStation(10);
  auto point = station->addPoint(11, IEC60870_5_TypeID::C_DC_TA_1);

  sCS101_AppLayerParameters appLayerParameters{.sizeOfTypeId = 1,
                                               .sizeOfVSQ = 0,
                                               .sizeOfCOT = 2,
                                               .originatorAddress = 99,
                                               .sizeOfCA = 2,
                                               .sizeOfIOA = 3,
                                               .maxSizeOfASDU = 249};
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CS101_ASDU asdu = CS101_ASDU_create(
      &appLayerParameters, false, CS101_COT_ACTIVATION, 0, 14, false, false);
  CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
  InformationObject io = reinterpret_cast<InformationObject>(
      DoubleCommandWithCP56Time2a_create(nullptr, 14, 1, false, 0, &time));
  CS101_ASDU_addInformationObject(asdu, io);
  auto message =
      Remote::Message::IncomingMessage::create(asdu, &appLayerParameters);

  point->onReceive(message);
  REQUIRE(std::get<DoublePointValue>(point->getValue()) ==
          IEC60870_DOUBLE_POINT_OFF);
  REQUIRE(point->getRecordedAt()->getTime() ==
          std::chrono::system_clock::time_point(
              std::chrono::milliseconds(1680517666000)));
  REQUIRE(std::get<std::monostate>(point->getQuality()) == std::monostate{});

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}
