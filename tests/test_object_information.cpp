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

#include "object/DataPoint.h"
#include "object/Station.h"
#include "types.h"

TEST_CASE("Create information", "[object::information]") {
  auto info1 = Object::SingleInfo::create(0);
  REQUIRE(info1->name().compare("SingleInfo") == 0);
  auto info2 = Object::SingleCmd::create(0);
  REQUIRE(info2->name().compare("SingleCmd") == 0);
  auto info3 = Object::DoubleInfo::create(0);
  REQUIRE(info3->name().compare("DoubleInfo") == 0);
  auto info4 = Object::DoubleCmd::create(0);
  REQUIRE(info4->name().compare("DoubleCmd") == 0);
  auto info5 = Object::StepInfo::create(0);
  REQUIRE(info5->name().compare("StepInfo") == 0);
  auto info6 = Object::StepCmd::create(0);
  REQUIRE(info6->name().compare("StepCmd") == 0);
  auto info7 = Object::BinaryInfo::create(0);
  REQUIRE(info7->name().compare("BinaryInfo") == 0);
  auto info8 = Object::BinaryCmd::create(0);
  REQUIRE(info8->name().compare("BinaryCmd") == 0);
  auto info9 = Object::NormalizedInfo::create(0);
  REQUIRE(info9->name().compare("NormalizedInfo") == 0);
  auto info10 = Object::NormalizedCmd::create(0);
  REQUIRE(info10->name().compare("NormalizedCmd") == 0);
  auto info11 = Object::ScaledInfo::create(0);
  REQUIRE(info11->name().compare("ScaledInfo") == 0);
  auto info12 = Object::ScaledCmd::create(0);
  REQUIRE(info12->name().compare("ScaledCmd") == 0);
  auto info13 = Object::ShortInfo::create(0);
  REQUIRE(info13->name().compare("ShortInfo") == 0);
  auto info14 = Object::ShortCmd::create(0);
  REQUIRE(info14->name().compare("ShortCmd") == 0);
  auto info15 = Object::BinaryCounterInfo::create(0);
  REQUIRE(info15->name().compare("BinaryCounterInfo") == 0);
  auto info16 = Object::ProtectionEquipmentEventInfo::create(0);
  REQUIRE(info16->name().compare("ProtectionEquipmentEventInfo") == 0);
  auto info17 = Object::ProtectionEquipmentStartEventsInfo::create(0);
  REQUIRE(info17->name().compare("ProtectionEquipmentStartEventsInfo") == 0);
  auto info18 = Object::ProtectionEquipmentOutputCircuitInfo::create(0);
  REQUIRE(info18->name().compare("ProtectionEquipmentOutputCircuitInfo") == 0);
  auto info19 = Object::StatusWithChangeDetection::create(0);
  REQUIRE(info19->name().compare("StatusWithChangeDetection") == 0);
}
