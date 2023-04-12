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

#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

TEST_CASE("Create point", "[object::point]") {
  auto point = Object::DataPoint::create(11, IEC60870_5_TypeID::M_SP_NA_1,
                                         nullptr, 0, 0, false);
  REQUIRE(point->getStation().get() == nullptr);
  REQUIRE(point->getInformationObjectAddress() == 11);
  REQUIRE(point->getRelatedInformationObjectAddress() == 0);
  REQUIRE(point->getRelatedInformationObjectAutoReturn() == false);
  REQUIRE(point->getType() == IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(point->getReportInterval_ms() == 0);
  REQUIRE(point->getQuality() == Quality::None);
  REQUIRE(point->getValue() == 0);
  REQUIRE(point->getValueAsInt32() == 0);
  REQUIRE(point->getValueAsUInt32() == 0);
  REQUIRE(point->getValueAsFloat() == 0);
  REQUIRE(point->getUpdatedAt_ms() == 0);
  REQUIRE(point->getReportedAt_ms() == 0);
  REQUIRE(point->getReceivedAt_ms() == 0);
  REQUIRE(point->getSentAt_ms() == 0);
}

TEST_CASE("Set point value", "[object::point]") {
  auto point = Object::DataPoint::create(11, IEC60870_5_TypeID::M_SP_NA_1,
                                         nullptr, 0, 0, false);

  point->setValueEx(56.78, Quality::None, 1234567890);
  REQUIRE(point->getValue() == 56.78);
  REQUIRE(point->getValueAsInt32() == 56);
  REQUIRE(point->getValueAsUInt32() == 56);
  REQUIRE(point->getValueAsFloat() == (float)56.78);
  REQUIRE(point->getUpdatedAt_ms() == 1234567890);
  // SinglePoint value must be of [0, 1]
  REQUIRE(point->getQuality() == Quality::Invalid);
}

TEST_CASE("Set point value via message", "[object::point]") {
  auto point = Object::DataPoint::create(11, IEC60870_5_TypeID::M_SP_NA_1,
                                         nullptr, 0, 0, false);

  sCS101_AppLayerParameters appLayerParameters{.sizeOfTypeId = 1,
                                               .sizeOfVSQ = 0,
                                               .sizeOfCOT = 2,
                                               .originatorAddress = 99,
                                               .sizeOfCA = 2,
                                               .sizeOfIOA = 3,
                                               .maxSizeOfASDU = 249};
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CS101_ASDU asdu = CS101_ASDU_create(
      &appLayerParameters, false, CS101_COT_SPONTANEOUS, 0, 14, false, false);
  CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
  InformationObject io = (InformationObject)SinglePointWithCP56Time2a_create(
      nullptr, 14, 1, 0, &time);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message =
      Remote::Message::IncomingMessage::create(asdu, &appLayerParameters);

  point->onReceive(message);
  REQUIRE(point->getValue() == 1);
  REQUIRE(point->getUpdatedAt_ms() == 1680517666000);
  // SinglePoint value must be of [0, 1]
  REQUIRE(point->getQuality() == Quality::None);

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}
