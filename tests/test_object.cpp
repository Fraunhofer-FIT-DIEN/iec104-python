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
 * @file test_object.cpp
 * @brief Coverage-driver tests for src/object/{Information,DataPoint,Station,
 *        DateTime}.cpp and src/remote/Helper.cpp.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

#include <pybind11/embed.h>
#include <pybind11/pybind11.h>

#include "Client.h"
#include "Server.h"
#include "object/DataPoint.h"
#include "object/DateTime.h"
#include "object/Information.h"
#include "object/Station.h"
#include "remote/Helper.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

namespace py = pybind11;

using namespace Object;
using namespace std::chrono_literals;

namespace {

using Catch::Approx;

// ---------------------------------------------------------------------------
// Python callbacks - defined in the embedded interpreter (main.cpp holds a
// py::scoped_interpreter). Signatures must match what DataPoint expects.
// ---------------------------------------------------------------------------
void ensurePythonCallbacks() {
  try {
    py::exec(R"(
import c104

def _cb_receive(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    return c104.ResponseState.SUCCESS

def _cb_receive_fail(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    return c104.ResponseState.FAILURE

def _cb_void(point: c104.Point) -> None:
    pass
)",
             py::globals());
  } catch (const std::exception &) {
    // interpreter unavailable - callback branches stay uncovered but nothing
    // may crash
  }
}

// The DataPoint callback setters take py::object& (non-const lvalue reference),
// so callers must bind to a named local variable before passing.
template <typename Fn> void withCallback(const char *name, Fn &&fn) {
  py::object cb = py::globals()[name];
  fn(cb);
}
template <typename Fn> void withReceiveCallback(Fn &&fn) {
  withCallback("_cb_receive", [&](py::object &cb) { fn(cb); });
}
template <typename Fn> void withReceiveFailCallback(Fn &&fn) {
  withCallback("_cb_receive_fail", [&](py::object &cb) { fn(cb); });
}
template <typename Fn> void withVoidCallback(Fn &&fn) {
  withCallback("_cb_void", [&](py::object &cb) { fn(cb); });
}

// Build an I-format APDU buffer (fully framed 104 message) of a given size.
// Fills control + ASDU header fields so the formatters can parse it.
std::vector<uint_fast8_t> makeIFrameBuffer(int size) {
  std::vector<uint_fast8_t> msg(static_cast<size_t>(size), 0);
  msg[0] = 0x68;                           // start octet
  msg[1] = static_cast<uint8_t>(size - 2); // length octet
  msg[IEC60870_TYPEID_OFFSET] =
      static_cast<uint8_t>(IEC60870_5_TypeID::M_SP_NA_1);
  msg[IEC60870_STRUCTURE_OFFSET] = 0x85; // sequence=1, 5 objects
  msg[IEC60870_MSGINFO_OFFSET] = 0x00;   // cot=0 (unknown), positive, not test
  msg[IEC60870_SOURCECA_OFFSET] = 0x00;
  msg[IEC60870_TARGETCA1_OFFSET] = 0x2F;
  msg[IEC60870_TARGETCA2_OFFSET] = 0x00;
  msg[IEC60870_OBJECT_OFFSET] = 0x0B;
  return msg;
}

} // namespace

// ===========================================================================
// 1. Information subtypes
// ===========================================================================

TEST_CASE("Information base & readonly behaviour", "[object::info]") {
  // base class through a concrete type
  auto si = SingleInfo::create(true, Quality::Invalid,
                               std::chrono::system_clock::now());
  REQUIRE(std::get<bool>(si->getValue()) == true);
  REQUIRE(std::get<Quality>(si->getQuality()) == Quality::Invalid);
  REQUIRE(si->getRecordedAt().has_value());
  REQUIRE(si->getProcessedAt() > std::chrono::system_clock::time_point::min());
  REQUIRE(si->isReadonly() == false);
  REQUIRE(si->toString().find("SingleInfo") != std::string::npos);

  // setRecordedAt / setProcessedAt / setReadonly
  si->setRecordedAt(std::nullopt);
  REQUIRE(si->getRecordedAt().has_value() == false);
  si->setRecordedAt(std::chrono::system_clock::now());
  auto tp = std::chrono::system_clock::now();
  si->setProcessedAt(tp);
  REQUIRE(si->getProcessedAt() >= tp - 1h);

  si->setReadonly();
  REQUIRE(si->isReadonly() == true);
  // readonly: setValue / setQuality throw domain_error
  REQUIRE_THROWS_AS(si->setValue(true), std::domain_error);
  REQUIRE_THROWS_AS(si->setQuality(InfoQuality{Quality::None}),
                    std::domain_error);
  // readonly: setRecordedAt is a no-op (does NOT change the existing value;
  // recorded_at was set above and remains set)
  si->setRecordedAt(std::chrono::system_clock::now());
  REQUIRE(si->getRecordedAt().has_value() == true);

  // setReadonly on an already-readonly object returns early
  si->setReadonly();

  // base Information base_toString via a raw info
  auto info = std::make_shared<Information>(std::nullopt, false);
  REQUIRE(info->name() == "Information");
  REQUIRE(info->toString().find("c104.Information") != std::string::npos);
  REQUIRE(std::holds_alternative<std::monostate>(info->getValue()));
  info->setValue(InfoValue{5});
  info->setQuality(InfoQuality{Quality::None});

  // Command base
  auto cmd = std::make_shared<Command>(std::nullopt, false);
  REQUIRE(cmd->name() == "Command");
  REQUIRE(cmd->isSelectable() == false);
  REQUIRE(cmd->isSelect() == false);
  REQUIRE(cmd->toString().find("c104.Command") != std::string::npos);
}

TEST_CASE("Information::setValue/setQuality bad_variant_access error",
          "[object::info]") {
  auto si = SingleInfo::create(false);
  // SingleInfo expects bool / Quality -> feed wrong variant types
  REQUIRE_THROWS_AS(si->setValue(InfoValue{float(1.5)}), std::invalid_argument);
  REQUIRE_THROWS_AS(si->setQuality(InfoQuality{BinaryCounterQuality::Invalid}),
                    std::invalid_argument);
}

TEST_CASE("SingleInfo/SingleCmd", "[object::info]") {
  auto si = SingleInfo::create(true, Quality::Overflow,
                               std::chrono::system_clock::now());
  REQUIRE(si->isOn() == true);
  REQUIRE(std::get<bool>(si->getValue()) == true);
  si->setValue(false);
  REQUIRE(std::get<bool>(si->getValue()) == false);
  si->setQuality(InfoQuality{Quality::Blocked});
  REQUIRE(std::get<Quality>(si->getQuality()) == Quality::Blocked);
  // bool_toString renders "False" (capital F)
  REQUIRE(si->toString().find("on=False") != std::string::npos);

  auto sc = SingleCmd::create(true, CS101_QualifierOfCommand::SHORT_PULSE,
                              std::chrono::system_clock::now());
  REQUIRE(sc->isOn() == true);
  REQUIRE(sc->isSelectable() == true);
  REQUIRE(sc->isSelect() == false);
  REQUIRE(sc->getQualifier() == CS101_QualifierOfCommand::SHORT_PULSE);
  sc->setValue(false);
  REQUIRE(std::get<bool>(sc->getValue()) == false);
  sc->setQuality(InfoQuality{Quality::Substituted});
  REQUIRE(std::holds_alternative<std::monostate>(sc->getQuality()));
  REQUIRE(sc->toString().find("qualifier=SHORT_PULSE") != std::string::npos);
}

TEST_CASE("DoubleInfo/DoubleCmd", "[object::info]") {
  auto di = DoubleInfo::create(IEC60870_DOUBLE_POINT_ON, Quality::Invalid,
                               std::chrono::system_clock::now());
  REQUIRE(di->getState() == IEC60870_DOUBLE_POINT_ON);
  REQUIRE(std::get<DoublePointValue>(di->getValue()) ==
          IEC60870_DOUBLE_POINT_ON);
  di->setValue(IEC60870_DOUBLE_POINT_INTERMEDIATE);
  REQUIRE(std::get<DoublePointValue>(di->getValue()) ==
          IEC60870_DOUBLE_POINT_INTERMEDIATE);
  di->setQuality(InfoQuality{Quality::NonTopical});
  REQUIRE(std::get<Quality>(di->getQuality()) == Quality::NonTopical);
  REQUIRE(di->toString().find("state=") != std::string::npos);

  auto dc = DoubleCmd::create(IEC60870_DOUBLE_POINT_OFF,
                              CS101_QualifierOfCommand::LONG_PULSE,
                              std::chrono::system_clock::now());
  REQUIRE(dc->isSelectable() == true);
  dc->setValue(IEC60870_DOUBLE_POINT_ON);
  REQUIRE(std::get<DoublePointValue>(dc->getValue()) ==
          IEC60870_DOUBLE_POINT_ON);
  dc->setQuality(InfoQuality{Quality::Invalid});
  // Commands do not carry a Quality: setQuality is a no-op and getQuality()
  // returns the base monostate alternative.
  REQUIRE(std::holds_alternative<std::monostate>(dc->getQuality()));
  REQUIRE(dc->toString().find("qualifier=LONG_PULSE") != std::string::npos);
}

TEST_CASE("StepInfo/StepCmd", "[object::info]") {
  auto sti = StepInfo::create(LimitedInt7(3), true, Quality::Invalid,
                              std::chrono::system_clock::now());
  REQUIRE(sti->getPosition().get() == 3);
  REQUIRE(sti->isTransient() == true);
  REQUIRE(std::get<LimitedInt7>(sti->getValue()).get() == 3);
  sti->setValue(LimitedInt7(-2));
  REQUIRE(std::get<LimitedInt7>(sti->getValue()).get() == -2);
  sti->setQuality(InfoQuality{Quality::None});
  REQUIRE(std::get<Quality>(sti->getQuality()) == Quality::None);
  REQUIRE(sti->toString().find("position=-2") != std::string::npos);

  auto stc = StepCmd::create(IEC60870_STEP_HIGHER,
                             CS101_QualifierOfCommand::SHORT_PULSE,
                             std::chrono::system_clock::now());
  REQUIRE(stc->getStep() == IEC60870_STEP_HIGHER);
  REQUIRE(stc->isSelectable() == true);
  stc->setValue(IEC60870_STEP_LOWER);
  REQUIRE(std::get<StepCommandValue>(stc->getValue()) == IEC60870_STEP_LOWER);
  stc->setQuality(InfoQuality{Quality::Blocked});
  REQUIRE(std::holds_alternative<std::monostate>(stc->getQuality()));
  REQUIRE(stc->toString().find("qualifier=SHORT_PULSE") != std::string::npos);
}

TEST_CASE("BinaryInfo/BinaryCmd (Byte32)", "[object::info]") {
  auto bi = BinaryInfo::create(Byte32(0xDEADBEEF), Quality::Substituted,
                               std::chrono::system_clock::now());
  REQUIRE(bi->getBlob().get() == 0xDEADBEEF);
  REQUIRE(std::get<Byte32>(bi->getValue()).get() == 0xDEADBEEF);
  bi->setValue(Byte32(0x1234));
  REQUIRE(std::get<Byte32>(bi->getValue()).get() == 0x1234);
  bi->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::get<Quality>(bi->getQuality()) == Quality::Invalid);
  REQUIRE(bi->toString().find("blob=") != std::string::npos);

  auto bc = BinaryCmd::create(Byte32(0xABCD), std::chrono::system_clock::now());
  REQUIRE(bc->getBlob().get() == 0xABCD);
  bc->setValue(Byte32(0x1));
  REQUIRE(std::get<Byte32>(bc->getValue()).get() == 0x1);
  bc->setQuality(InfoQuality{Quality::None});
  REQUIRE(bc->toString().find("blob=") != std::string::npos);
}

TEST_CASE("NormalizedInfo/NormalizedCmd", "[object::info]") {
  auto ni = NormalizedInfo::create(NormalizedFloat(0.5f), Quality::Invalid,
                                   std::chrono::system_clock::now());
  REQUIRE(std::get<NormalizedFloat>(ni->getValue()).get() == Approx(0.5f));
  ni->setValue(NormalizedFloat(-1.0f));
  REQUIRE(std::get<NormalizedFloat>(ni->getValue()).get() == Approx(-1.0f));
  ni->setQuality(InfoQuality{Quality::Overflow});
  REQUIRE(std::get<Quality>(ni->getQuality()) == Quality::Overflow);
  REQUIRE(ni->toString().find("actual=") != std::string::npos);

  auto nc = NormalizedCmd::create(NormalizedFloat(0.25f), LimitedUInt7(1),
                                  std::chrono::system_clock::now());
  REQUIRE(nc->isSelectable() == true);
  REQUIRE(nc->getQualifier().get() == 1);
  nc->setValue(NormalizedFloat(1.0f));
  REQUIRE(std::get<NormalizedFloat>(nc->getValue()).get() == Approx(1.0f));
  nc->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::holds_alternative<std::monostate>(nc->getQuality()));
  REQUIRE(nc->toString().find("target=") != std::string::npos);
}

TEST_CASE("ScaledInfo/ScaledCmd", "[object::info]") {
  auto si = ScaledInfo::create(LimitedInt16(334), Quality::Invalid,
                               std::chrono::system_clock::now());
  REQUIRE(std::get<LimitedInt16>(si->getValue()).get() == 334);
  si->setValue(LimitedInt16(-7));
  REQUIRE(std::get<LimitedInt16>(si->getValue()).get() == -7);
  si->setQuality(InfoQuality{Quality::Blocked});
  REQUIRE(std::get<Quality>(si->getQuality()) == Quality::Blocked);
  REQUIRE(si->toString().find("actual=-7") != std::string::npos);

  auto sc = ScaledCmd::create(LimitedInt16(100), LimitedUInt7(3),
                              std::chrono::system_clock::now());
  REQUIRE(sc->getQualifier().get() == 3);
  sc->setValue(LimitedInt16(200));
  REQUIRE(std::get<LimitedInt16>(sc->getValue()).get() == 200);
  sc->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::holds_alternative<std::monostate>(sc->getQuality()));
  REQUIRE(sc->toString().find("target=") != std::string::npos);
}

TEST_CASE("ShortInfo/ShortCmd", "[object::info]") {
  auto si = ShortInfo::create(3.14f, Quality::NonTopical,
                              std::chrono::system_clock::now());
  REQUIRE(si->getActual() == Approx(3.14f));
  REQUIRE(std::get<float>(si->getValue()) == Approx(3.14f));
  si->setValue(float(1.5));
  REQUIRE(std::get<float>(si->getValue()) == Approx(1.5f));
  si->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::get<Quality>(si->getQuality()) == Quality::Invalid);
  REQUIRE(si->toString().find("actual=") != std::string::npos);

  auto sc =
      ShortCmd::create(9.9f, LimitedUInt7(5), std::chrono::system_clock::now());
  REQUIRE(sc->getTarget() == Approx(9.9f));
  REQUIRE(sc->getQualifier().get() == 5);
  sc->setValue(float(2.25));
  REQUIRE(std::get<float>(sc->getValue()) == Approx(2.25f));
  sc->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::holds_alternative<std::monostate>(sc->getQuality()));
  REQUIRE(sc->toString().find("target=") != std::string::npos);
}

TEST_CASE("BinaryCounterInfo", "[object::info]") {
  auto bi = BinaryCounterInfo::create(42, LimitedUInt5(7),
                                      BinaryCounterQuality::Adjusted,
                                      std::chrono::system_clock::now());
  REQUIRE(bi->getCounter() == 42);
  REQUIRE(bi->getSequence().get() == 7);
  REQUIRE(std::get<int32_t>(bi->getValue()) == 42);
  bi->setValue(int32_t(-5));
  REQUIRE(std::get<int32_t>(bi->getValue()) == -5);
  bi->setQuality(InfoQuality{BinaryCounterQuality::Carry});
  REQUIRE(std::get<BinaryCounterQuality>(bi->getQuality()) ==
          BinaryCounterQuality::Carry);
  REQUIRE(bi->toString().find("counter=-5") != std::string::npos);
}

TEST_CASE("ProtectionEquipmentEventInfo", "[object::info]") {
  auto ei = ProtectionEquipmentEventInfo::create(
      IEC60870_EVENTSTATE_ON, LimitedUInt16(500), Quality::ElapsedTimeInvalid,
      std::chrono::system_clock::now());
  REQUIRE(ei->getState() == IEC60870_EVENTSTATE_ON);
  REQUIRE(ei->getElapsed_ms().get() == 500);
  REQUIRE(std::get<EventState>(ei->getValue()) == IEC60870_EVENTSTATE_ON);
  ei->setValue(IEC60870_EVENTSTATE_OFF);
  REQUIRE(std::get<EventState>(ei->getValue()) == IEC60870_EVENTSTATE_OFF);
  ei->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::get<Quality>(ei->getQuality()) == Quality::Invalid);
  REQUIRE(ei->toString().find("state=") != std::string::npos);
}

TEST_CASE("ProtectionEquipmentStartEventsInfo", "[object::info]") {
  auto si = ProtectionEquipmentStartEventsInfo::create(
      StartEvents::General, LimitedUInt16(250), Quality::Invalid,
      std::chrono::system_clock::now());
  REQUIRE(si->getEvents() == StartEvents::General);
  REQUIRE(si->getRelayDuration_ms().get() == 250);
  REQUIRE(std::get<StartEvents>(si->getValue()) == StartEvents::General);
  si->setValue(StartEvents::PhaseL1);
  REQUIRE(std::get<StartEvents>(si->getValue()) == StartEvents::PhaseL1);
  si->setQuality(InfoQuality{Quality::None});
  REQUIRE(std::get<Quality>(si->getQuality()) == Quality::None);
  REQUIRE(si->toString().find("events=") != std::string::npos);
}

TEST_CASE("ProtectionEquipmentOutputCircuitInfo", "[object::info]") {
  auto oi = ProtectionEquipmentOutputCircuitInfo::create(
      OutputCircuits::PhaseL2, LimitedUInt16(750), Quality::Invalid,
      std::chrono::system_clock::now());
  REQUIRE(oi->getCircuits() == OutputCircuits::PhaseL2);
  REQUIRE(oi->getRelayOperating_ms().get() == 750);
  REQUIRE(std::get<OutputCircuits>(oi->getValue()) == OutputCircuits::PhaseL2);
  oi->setValue(OutputCircuits::General);
  REQUIRE(std::get<OutputCircuits>(oi->getValue()) == OutputCircuits::General);
  oi->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::get<Quality>(oi->getQuality()) == Quality::Invalid);
  REQUIRE(oi->toString().find("circuits=") != std::string::npos);
}

TEST_CASE("StatusWithChangeDetection", "[object::info]") {
  auto si = StatusWithChangeDetection::create(FieldSet16::I3, FieldSet16::I5,
                                              Quality::Invalid,
                                              std::chrono::system_clock::now());
  REQUIRE(si->getStatus() == FieldSet16::I3);
  REQUIRE(si->getChanged() == FieldSet16::I5);
  REQUIRE(std::get<FieldSet16>(si->getValue()) == FieldSet16::I3);
  si->setValue(FieldSet16::I9);
  REQUIRE(std::get<FieldSet16>(si->getValue()) == FieldSet16::I9);
  si->setQuality(InfoQuality{Quality::None});
  REQUIRE(std::get<Quality>(si->getQuality()) == Quality::None);
  REQUIRE(si->toString().find("status=") != std::string::npos);
}

// ===========================================================================
// 2. DataPoint
// ===========================================================================

TEST_CASE("DataPoint: construct every monitoring + command type",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  struct TC {
    IEC60870_5_TypeID type;
    bool wantsTimestamp; // *_TB / *_TA / *_TC / *_TD / *_TE / *_TF types
  };
  const TC cases[] = {
      {IEC60870_5_TypeID::M_SP_NA_1, false},
      {IEC60870_5_TypeID::M_SP_TB_1, true},
      {IEC60870_5_TypeID::C_SC_NA_1, false},
      {IEC60870_5_TypeID::C_SC_TA_1, true},
      {IEC60870_5_TypeID::M_DP_NA_1, false},
      {IEC60870_5_TypeID::M_DP_TB_1, true},
      {IEC60870_5_TypeID::C_DC_NA_1, false},
      {IEC60870_5_TypeID::C_DC_TA_1, true},
      {IEC60870_5_TypeID::M_ST_NA_1, false},
      {IEC60870_5_TypeID::M_ST_TB_1, true},
      {IEC60870_5_TypeID::C_RC_NA_1, false},
      {IEC60870_5_TypeID::C_RC_TA_1, true},
      {IEC60870_5_TypeID::M_ME_NA_1, false},
      {IEC60870_5_TypeID::M_ME_ND_1, false},
      {IEC60870_5_TypeID::M_ME_TD_1, true},
      {IEC60870_5_TypeID::C_SE_NA_1, false},
      {IEC60870_5_TypeID::C_SE_TA_1, true},
      {IEC60870_5_TypeID::M_ME_NB_1, false},
      {IEC60870_5_TypeID::M_ME_TE_1, true},
      {IEC60870_5_TypeID::C_SE_NB_1, false},
      {IEC60870_5_TypeID::C_SE_TB_1, true},
      {IEC60870_5_TypeID::M_ME_NC_1, false},
      {IEC60870_5_TypeID::M_ME_TF_1, true},
      {IEC60870_5_TypeID::C_SE_NC_1, false},
      {IEC60870_5_TypeID::C_SE_TC_1, true},
      {IEC60870_5_TypeID::M_BO_NA_1, false},
      {IEC60870_5_TypeID::M_BO_TB_1, true},
      {IEC60870_5_TypeID::C_BO_NA_1, false},
      {IEC60870_5_TypeID::C_BO_TA_1, true},
      {IEC60870_5_TypeID::M_IT_NA_1, false},
      {IEC60870_5_TypeID::M_IT_TB_1, true},
      {IEC60870_5_TypeID::M_EP_TD_1, true},
      {IEC60870_5_TypeID::M_EP_TE_1, true},
      {IEC60870_5_TypeID::M_EP_TF_1, true},
      {IEC60870_5_TypeID::M_PS_NA_1, false},
  };

  uint_fast32_t ioa = 101;
  for (const auto &c : cases) {
    auto p = station->addPoint(ioa, c.type);
    REQUIRE(p != nullptr);
    REQUIRE(p->getType() == c.type);
    REQUIRE(p->getInformationObjectAddress() == ioa);
    REQUIRE(p->getStation().get() == station.get());
    // a timestamped type should have a recorded_at after first construction
    if (c.wantsTimestamp) {
      REQUIRE(p->getRecordedAt().has_value());
    }
    REQUIRE(p->getInfo() != nullptr);
    REQUIRE(p->toString().find("io_address=") != std::string::npos);
    ++ioa;
  }
}

TEST_CASE("DataPoint: constructor error branches", "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // unsupported type (>= M_EI_NA_1)
  REQUIRE_THROWS_AS(station->addPoint(1, IEC60870_5_TypeID::M_EI_NA_1),
                    std::invalid_argument);

  // out-of-range IOA
  REQUIRE_THROWS_AS(station->addPoint(0x1000000, IEC60870_5_TypeID::M_SP_NA_1),
                    std::invalid_argument);

  // related_ioa out of range
  REQUIRE_THROWS_AS(station->addPoint(2, IEC60870_5_TypeID::C_SC_NA_1, 0,
                                      std::optional<uint_fast32_t>(0x1000000)),
                    std::invalid_argument);

  // auto_return with no related set: the missing related IOA defaults to the
  // out-of-range UNDEFINED sentinel, so validation fires -> throw
  REQUIRE_THROWS_AS(
      station->addPoint(3, IEC60870_5_TypeID::C_SC_NA_1, 0, std::nullopt, true),
      std::invalid_argument);

  // auto_return only for control types
  REQUIRE_THROWS_AS(station->addPoint(4, IEC60870_5_TypeID::M_SP_NA_1, 0,
                                      std::optional<uint_fast32_t>(100), true),
                    std::invalid_argument);

  // report interval invalid (not multiple of tick rate)
  REQUIRE_THROWS_AS(station->addPoint(5, IEC60870_5_TypeID::M_SP_NA_1, 33),
                    std::range_error);

  // report interval only allowed for monitoring types (control type)
  REQUIRE_THROWS_AS(station->addPoint(6, IEC60870_5_TypeID::C_SC_NA_1, 100),
                    std::invalid_argument);
}

TEST_CASE("DataPoint: value & quality set/get across types",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // bool -> SingleInfo (M_SP_NA_1)
  auto sp = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  sp->setValue(true);
  REQUIRE(std::get<bool>(sp->getValue()) == true);
  sp->setQuality(InfoQuality{Quality::Invalid});
  REQUIRE(std::get<Quality>(sp->getQuality()) == Quality::Invalid);

  // float -> ShortInfo (M_ME_NC_1)
  auto sh = station->addPoint(2, IEC60870_5_TypeID::M_ME_NC_1);
  sh->setValue(float(3.5));
  REQUIRE(std::get<float>(sh->getValue()) == Approx(3.5f));
  sh->setQuality(InfoQuality{Quality::Blocked});
  REQUIRE(std::get<Quality>(sh->getQuality()) == Quality::Blocked);

  // LimitedInt16 -> ScaledInfo (M_ME_NB_1)
  auto sc = station->addPoint(3, IEC60870_5_TypeID::M_ME_NB_1);
  sc->setValue(LimitedInt16(77));
  REQUIRE(std::get<LimitedInt16>(sc->getValue()).get() == 77);

  // NormalizedFloat -> NormalizedInfo (M_ME_NA_1)
  auto nf = station->addPoint(4, IEC60870_5_TypeID::M_ME_NA_1);
  nf->setValue(NormalizedFloat(0.9f));
  REQUIRE(std::get<NormalizedFloat>(nf->getValue()).get() == Approx(0.9f));

  // DoublePointValue -> DoubleInfo (M_DP_NA_1)
  auto dp = station->addPoint(5, IEC60870_5_TypeID::M_DP_NA_1);
  dp->setValue(IEC60870_DOUBLE_POINT_ON);
  REQUIRE(std::get<DoublePointValue>(dp->getValue()) ==
          IEC60870_DOUBLE_POINT_ON);

  // LimitedInt7 -> StepInfo (M_ST_NA_1)
  auto st = station->addPoint(6, IEC60870_5_TypeID::M_ST_NA_1);
  st->setValue(LimitedInt7(2));
  REQUIRE(std::get<LimitedInt7>(st->getValue()).get() == 2);

  // Byte32 -> BinaryInfo (M_BO_NA_1)
  auto bi = station->addPoint(7, IEC60870_5_TypeID::M_BO_NA_1);
  bi->setValue(Byte32(0xFF));
  REQUIRE(std::get<Byte32>(bi->getValue()).get() == 0xFF);

  // int32_t -> BinaryCounterInfo (M_IT_NA_1)
  auto it = station->addPoint(8, IEC60870_5_TypeID::M_IT_NA_1);
  it->setValue(int32_t(123));
  REQUIRE(std::get<int32_t>(it->getValue()) == 123);

  // setValue / setQuality wrong type -> invalid_argument via information
  REQUIRE_THROWS_AS(sp->setValue(InfoValue{float(1.0)}), std::invalid_argument);
  REQUIRE_THROWS_AS(sh->setQuality(InfoQuality{BinaryCounterQuality::Invalid}),
                    std::invalid_argument);

  // Command type value (SingleCmd C_SC_NA_1)
  auto cmd = station->addPoint(9, IEC60870_5_TypeID::C_SC_NA_1);
  cmd->setValue(true);
  REQUIRE(std::get<bool>(cmd->getValue()) == true);
}

TEST_CASE("DataPoint: setInfo (timestamp drop/inject & type validation)",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  // M_SP_NA_1 drops timestamp
  auto sp = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  auto withTs =
      SingleInfo::create(true, Quality::None, std::chrono::system_clock::now());
  sp->setInfo(withTs);
  REQUIRE(sp->getRecordedAt().has_value() == false);

  // M_SP_TB_1 injects timestamp
  auto spb = station->addPoint(2, IEC60870_5_TypeID::M_SP_TB_1);
  auto noTs = SingleInfo::create(true);
  spb->setInfo(noTs);
  REQUIRE(spb->getRecordedAt().has_value() == true);

  // wrong information type -> throw
  REQUIRE_THROWS_AS(sp->setInfo(DoubleInfo::create(IEC60870_DOUBLE_POINT_OFF)),
                    std::invalid_argument);

  // command type setInfo
  auto cmd = station->addPoint(3, IEC60870_5_TypeID::C_SC_NA_1);
  auto cmdi = SingleCmd::create(true, CS101_QualifierOfCommand::SHORT_PULSE,
                                std::chrono::system_clock::now());
  cmd->setInfo(cmdi);
  REQUIRE(cmd->getRecordedAt().has_value() == false);

  // normalization type setInfo
  auto nf = station->addPoint(4, IEC60870_5_TypeID::M_ME_NA_1);
  auto nfi = NormalizedInfo::create(NormalizedFloat(1.0f), Quality::None,
                                    std::chrono::system_clock::now());
  nf->setInfo(nfi);
  REQUIRE(nf->getRecordedAt().has_value() == false);

  // ScaledInfo setInfo (M_ME_NB_1)
  auto sc = station->addPoint(5, IEC60870_5_TypeID::M_ME_NB_1);
  auto sci = ScaledInfo::create(LimitedInt16(5));
  sc->setInfo(sci);
  REQUIRE(sc->getRecordedAt().has_value() == false);

  // protection event type setInfo (M_EP_TD_1)
  auto pe = station->addPoint(6, IEC60870_5_TypeID::M_EP_TD_1);
  auto pei = ProtectionEquipmentEventInfo::create(IEC60870_EVENTSTATE_ON);
  pe->setInfo(pei);
  REQUIRE(pe->getRecordedAt().has_value() == true);

  // status & change type setInfo (M_PS_NA_1)
  auto ps = station->addPoint(7, IEC60870_5_TypeID::M_PS_NA_1);
  auto psi = StatusWithChangeDetection::create(
      FieldSet16::I1, FieldSet16::I2, Quality::None,
      std::chrono::system_clock::now());
  ps->setInfo(psi);
  REQUIRE(ps->getRecordedAt().has_value() == false);
}

TEST_CASE("DataPoint: report interval / timer scheduling", "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(p->getReportInterval_ms() == 0);
  // 0 => no scheduling
  REQUIRE(p->nextReportAt().has_value() == false);

  // valid multiple of tick rate (100)
  p->setReportInterval_ms(100);
  REQUIRE(p->getReportInterval_ms() == 100);
  REQUIRE(p->nextReportAt().has_value() == true);

  // invalid (<tick rate) -> range_error
  REQUIRE_THROWS_AS(p->setReportInterval_ms(50), std::range_error);
  // invalid (not a multiple) -> range_error
  REQUIRE_THROWS_AS(p->setReportInterval_ms(150), std::range_error);
  // non-monitoring type -> invalid_argument
  auto cmd = station->addPoint(2, IEC60870_5_TypeID::C_SC_NA_1);
  REQUIRE_THROWS_AS(cmd->setReportInterval_ms(100), std::invalid_argument);

  // timer interval default 0 => no timer
  REQUIRE(p->getTimerInterval_ms() == 0);
  REQUIRE(p->nextTimerAt().has_value() == false);
  // invalid timer interval -> range_error
  ensurePythonCallbacks();
  {
    py::object cb = py::globals()["_cb_void"];
    REQUIRE_THROWS_AS(p->setOnTimerCallback(cb, 30), std::range_error);
  }
}

TEST_CASE("DataPoint: related info & command mode", "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);

  auto cmd = station->addPoint(1, IEC60870_5_TypeID::C_SC_NA_1);
  // a freshly created point reports no related IOA (nullopt)
  REQUIRE(cmd->getRelatedInformationObjectAddress().has_value() == false);
  cmd->setRelatedInformationObjectAddress(50);
  REQUIRE(cmd->getRelatedInformationObjectAddress().value() == 50);
  cmd->setRelatedInformationObjectAddress(std::nullopt);
  REQUIRE(cmd->getRelatedInformationObjectAddress().has_value() == false);

  // set related with auto return
  cmd->setRelatedInformationObjectAutoReturn(false);
  REQUIRE(cmd->getRelatedInformationObjectAutoReturn() == false);

  // A monitoring point with a related IO + auto return enabled
  auto mon = station->addPoint(2, IEC60870_5_TypeID::M_SP_NA_1, 0,
                               std::optional<uint_fast32_t>(60));
  REQUIRE(mon->getRelatedInformationObjectAddress().value() == 60);
  // auto return only for control types -> throw on monitoring point
  REQUIRE_THROWS_AS(mon->setRelatedInformationObjectAutoReturn(true),
                    std::invalid_argument);
  // auto return without related -> throw
  REQUIRE_THROWS_AS(cmd->setRelatedInformationObjectAutoReturn(true),
                    std::invalid_argument);
  // invalid related IOA -> throw
  REQUIRE_THROWS_AS(cmd->setRelatedInformationObjectAddress(
                        std::optional<uint_fast32_t>(0x1000000)),
                    std::invalid_argument);

  // command mode
  REQUIRE(cmd->getCommandMode() == DIRECT_COMMAND);
  cmd->setCommandMode(SELECT_AND_EXECUTE_COMMAND);
  REQUIRE(cmd->getCommandMode() == SELECT_AND_EXECUTE_COMMAND);
  cmd->setCommandMode(DIRECT_COMMAND);

  // select-and-execute not allowed on C_BO
  auto bo = station->addPoint(3, IEC60870_5_TypeID::C_BO_NA_1);
  REQUIRE_THROWS_AS(bo->setCommandMode(SELECT_AND_EXECUTE_COMMAND),
                    std::invalid_argument);

  // getSelectedByOriginatorAddress (no selection -> nullopt)
  REQUIRE(cmd->getSelectedByOriginatorAddress().has_value() == false);
}

TEST_CASE("DataPoint: callbacks (server context)", "[object::point]") {
  ensurePythonCallbacks();
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  // on_receive callback -> returns SUCCESS
  withReceiveCallback([&](py::object &cb) { p->setOnReceiveCallback(cb); });
  REQUIRE_NOTHROW(p->onBeforeRead());

  // on_before_read
  withVoidCallback([&](py::object &cb) { p->setOnBeforeReadCallback(cb); });
  p->onBeforeRead();

  // on_before_auto_transmit
  withVoidCallback(
      [&](py::object &cb) { p->setOnBeforeAutoTransmitCallback(cb); });
  p->onBeforeAutoTransmit();

  // on_timer with valid interval (multiple of 100)
  withVoidCallback([&](py::object &cb) { p->setOnTimerCallback(cb, 100); });
  REQUIRE(p->getTimerInterval_ms() == 100);
  REQUIRE(p->nextTimerAt().has_value() == true);
  p->onTimer();
}

TEST_CASE("DataPoint: on_receive failure result & bad callback sig",
          "[object::point]") {
  ensurePythonCallbacks();
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  // on_receive returning FAILURE
  withReceiveFailCallback([&](py::object &cb) { p->setOnReceiveCallback(cb); });

  sCS101_AppLayerParameters app{.sizeOfTypeId = 1,
                                .sizeOfVSQ = 0,
                                .sizeOfCOT = 2,
                                .originatorAddress = 0,
                                .sizeOfCA = 2,
                                .sizeOfIOA = 3,
                                .maxSizeOfASDU = 249};
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
  InformationObject io = (InformationObject)SinglePointInformation_create(
      nullptr, 1, true, IEC60870_QUALITY_GOOD);
  CS101_ASDU asdu = CS101_ASDU_create(&app, false, CS101_COT_SPONTANEOUS, 0, 10,
                                      false, false);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message = Remote::Message::IncomingMessage::create(asdu, &app);

  auto result = p->onReceive(message);
  REQUIRE(result == RESPONSE_STATE_FAILURE);

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("DataPoint: read() throw branches", "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto sp = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  // server context -> cannot send read command
  REQUIRE_THROWS_AS(sp->read(), std::invalid_argument);

  // station reference deleted -> throw
  sp->detach();
  REQUIRE_THROWS_AS(sp->read(), std::invalid_argument);
}

TEST_CASE("DataPoint: read() client context via connection",
          "[object::point]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 2404);
  auto station = connection->addStation(20);

  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(p->getStation().get() == station.get());
  REQUIRE(p->getStation()->isLocal() == false);

  // originator->read on client: connection->read is invoked (may return false
  // as the socket is not open, must not throw)
  REQUIRE_NOTHROW(p->read());

  // setOnBeforeReadCallback as client -> throw
  ensurePythonCallbacks();
  {
    py::object cb = py::globals()["_cb_void"];
    REQUIRE_THROWS_AS(p->setOnBeforeReadCallback(cb), std::invalid_argument);
  }
  // setOnBeforeAutoTransmitCallback as client -> throw
  {
    py::object cb = py::globals()["_cb_void"];
    REQUIRE_THROWS_AS(p->setOnBeforeAutoTransmitCallback(cb),
                      std::invalid_argument);
  }

  // client transmit with originator != 0 -> throw
  REQUIRE_THROWS_AS(p->transmit(CS101_COT_SPONTANEOUS, 1),
                    std::invalid_argument);

  // client connection deleted -> throw on transmit
  p->detach();
  REQUIRE_THROWS_AS(p->transmit(CS101_COT_SPONTANEOUS, 0),
                    std::invalid_argument);
}

TEST_CASE("DataPoint: transmit server context", "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto sp = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  // server transmit (monitoring) - send() returns false when server not
  // running w/ connections, but must not throw
  REQUIRE_NOTHROW(sp->transmit(CS101_COT_SPONTANEOUS, 0));

  // server transmit (control type, lazy confirm path)
  auto cmd = station->addPoint(2, IEC60870_5_TypeID::C_SC_NA_1);
  REQUIRE_NOTHROW(cmd->transmit(CS101_COT_ACTIVATION, 0));

  // station deleted -> throw
  cmd->detach();
  REQUIRE_THROWS_AS(cmd->transmit(CS101_COT_ACTIVATION, 0),
                    std::invalid_argument);
}

TEST_CASE("DataPoint: onReceive (no callback) via crafted ASDU",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto p = station->addPoint(1, IEC60870_5_TypeID::C_DC_TA_1);

  sCS101_AppLayerParameters app{.sizeOfTypeId = 1,
                                .sizeOfVSQ = 0,
                                .sizeOfCOT = 2,
                                .originatorAddress = 99,
                                .sizeOfCA = 2,
                                .sizeOfIOA = 3,
                                .maxSizeOfASDU = 249};
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CS101_ASDU asdu =
      CS101_ASDU_create(&app, false, CS101_COT_ACTIVATION, 0, 14, false, false);
  CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
  InformationObject io = (InformationObject)DoubleCommandWithCP56Time2a_create(
      nullptr, 14, 2, false, 0, &time);
  CS101_ASDU_addInformationObject(asdu, io);
  auto message = Remote::Message::IncomingMessage::create(asdu, &app);

  auto result = p->onReceive(message);
  REQUIRE(result == RESPONSE_STATE_SUCCESS);
  REQUIRE(std::get<DoublePointValue>(p->getValue()) ==
          IEC60870_DOUBLE_POINT_ON);
  REQUIRE(p->getRecordedAt() == std::chrono::system_clock::time_point(
                                    std::chrono::milliseconds(1680517666000)));

  InformationObject_destroy(io);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("DataPoint: processedAt & detach", "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);

  auto tp = std::chrono::system_clock::now();
  p->setProcessedAt(tp);
  REQUIRE(p->getProcessedAt() >= tp - 1h);
  REQUIRE(p->getProcessedAt() <= tp + 1h);

  // getStation after detach -> nullptr
  p->detach();
  REQUIRE(p->getStation() == nullptr);
}

TEST_CASE("DataPoint: server/client tick-rate forwarding via Station::addPoint",
          "[object::point]") {
  // server path (tickRate 100) -> report interval must be multiple of 100
  auto server = Server::create("127.0.0.1", 2404, 100);
  auto station = server->addStation(10);
  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1, 100);
  REQUIRE(p->getReportInterval_ms() == 100);
}

// ===========================================================================
// 3. Station
// ===========================================================================

TEST_CASE("Station: standalone create + getters", "[object::station]") {
  auto st = Object::Station::create(14, nullptr, nullptr);
  REQUIRE(st->getCommonAddress() == 14);
  REQUIRE(st->getServer() == nullptr);
  REQUIRE(st->getConnection() == nullptr);
  REQUIRE(st->hasPoints() == false);
  REQUIRE(st->getPoints().empty());
  // getPoint returns nullptr for not-found addresses
  REQUIRE(st->getPoint(0) == nullptr);
  REQUIRE(st->getPoint(55) == nullptr);
  REQUIRE(st->isLocal() == false);
  REQUIRE(st->toString().find("common_address=14") != std::string::npos);

  // not a server -> sendEndOfInitialization throws
  REQUIRE_THROWS_AS(
      st->sendEndOfInitialization(CS101_CauseOfInitialization::LOCAL_POWER_ON),
      std::runtime_error);
}

TEST_CASE("Station: via server (addStation)", "[object::station]") {
  auto server = Server::create("127.0.0.1", 2404, 100);
  // sendEndOfInitialization enqueues on the CS104 slave's redundancy-group
  // queue, which only exists once the server is started; on a never-started
  // slave the queue is NULL and lib60870 segfaults.
  server->start();
  auto st = server->addStation(20);
  REQUIRE(st != nullptr);
  REQUIRE(st->getServer().get() == server.get());
  REQUIRE(st->isLocal() == true);
  REQUIRE(st->getConnection() == nullptr);

  // hasPoints / getPoints / getPoint
  REQUIRE(st->hasPoints() == false);
  auto p1 = st->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  auto p2 = st->addPoint(2, IEC60870_5_TypeID::M_ME_NC_1);
  REQUIRE(st->hasPoints() == true);
  REQUIRE(st->getPoints().size() == 2);
  REQUIRE(st->getPoint(1).get() == p1.get());
  REQUIRE(st->getPoint(2).get() == p2.get());

  // duplicate addPoint -> nullptr
  REQUIRE(st->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1) == nullptr);

  // removePoint found -> true; not found -> false
  REQUIRE(st->removePoint(1) == true);
  REQUIRE(st->removePoint(1) == false);
  REQUIRE(st->getPoints().size() == 1);

  // sendEndOfInitialization works for server station
  REQUIRE_NOTHROW(
      st->sendEndOfInitialization(CS101_CauseOfInitialization::LOCAL_POWER_ON));

  // detach resets server -> no longer local
  st->detach();
  REQUIRE(st->isLocal() == false);
  REQUIRE(st->getServer() == nullptr);
  REQUIRE_THROWS_AS(
      st->sendEndOfInitialization(CS101_CauseOfInitialization::LOCAL_POWER_ON),
      std::runtime_error);

  // clean up the running server (stops the listener thread + frees the port)
  server->stop();
}

TEST_CASE("Station: via connection (client context)", "[object::station]") {
  auto client = Client::create();
  auto connection = client->addConnection("127.0.0.1", 2404);
  auto st = connection->addStation(30);
  REQUIRE(st != nullptr);
  REQUIRE(st->getServer() == nullptr);
  REQUIRE(st->getConnection().get() == connection.get());
  REQUIRE(st->isLocal() == false);

  auto p = st->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(p != nullptr);
  REQUIRE(st->getPoints().size() == 1);

  REQUIRE_THROWS_AS(
      st->sendEndOfInitialization(CS101_CauseOfInitialization::LOCAL_POWER_ON),
      std::runtime_error);

  // connection deleted but station alive -> isLocal false, getConnection null.
  // The connection is owned by the client, so the client must be dropped for
  // the station's weak_ptr to the connection to expire.
  connection.reset();
  client.reset();
  REQUIRE(st->getConnection() == nullptr);
  REQUIRE_THROWS_AS(p->transmit(CS101_COT_SPONTANEOUS, 0),
                    std::invalid_argument);
}

TEST_CASE("Station: standalone add/drop points & destructor",
          "[object::station]") {
  auto st = Object::Station::create(40, nullptr, nullptr);
  auto p = st->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1);
  REQUIRE(p->getStation().get() == st.get());
  // tickRate_ms is 0 without server/connection -> a nonzero report interval
  // would be an invalid multiple; report_ms=0 is fine
  REQUIRE(st->addPoint(2, IEC60870_5_TypeID::M_ME_NC_1) != nullptr);
  REQUIRE(st->getPoints().size() == 2);
  REQUIRE(st->removePoint(1) == true);
  REQUIRE(st->removePoint(2) == true);
  REQUIRE(st->getPoints().size() == 0);
}

// ===========================================================================
// 4. DateTime
// ===========================================================================

TEST_CASE("DateTime: default + time_point constructor", "[object::datetime]") {
  ::DateTime dt;
  REQUIRE(dt.getTime() > std::chrono::system_clock::time_point::min());
  REQUIRE(dt.isSubstituted() == false);
  REQUIRE(dt.isInvalid() == false);
  REQUIRE(dt.isSummertime() == false);

  auto tp = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(1234567890));
  ::DateTime dt2(tp);
  REQUIRE(dt2.getTime() == tp);
}

TEST_CASE("DateTime: CP56Time2a constructor", "[object::datetime]") {
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  // set invalid + substituted + summertime bits
  time.encodedValue[2] |= 0xC0; // minute bits 6+7 = substituted+invalid
  time.encodedValue[3] |= 0x80; // hour bit 7 = summertime
  ::DateTime dt(&time);
  REQUIRE(dt.isSubstituted() == true);
  REQUIRE(dt.isInvalid() == true);
  REQUIRE(dt.isSummertime() == true);

  // valid time
  sCP56Time2a time2{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CP56Time2a_createFromMsTimestamp(&time2, 1680517666000);
  ::DateTime dt2(&time2);
  REQUIRE(dt2.getTime() == std::chrono::system_clock::time_point(
                               std::chrono::milliseconds(1680517666000)));
}

// ===========================================================================
// 5. Remote::Helper
// ===========================================================================

TEST_CASE("Helper: isSingleCommonAddress / isGlobalCommonAddress",
          "[remote::helper]") {
  REQUIRE(Remote::isSingleCommonAddress(1) == true);
  REQUIRE(Remote::isSingleCommonAddress(0) == false);
  REQUIRE(Remote::isSingleCommonAddress(IEC60870_GLOBAL_COMMON_ADDRESS) ==
          false);
  REQUIRE(Remote::isGlobalCommonAddress(IEC60870_GLOBAL_COMMON_ADDRESS) ==
          true);
  REQUIRE(Remote::isGlobalCommonAddress(1) == false);
}

TEST_CASE("Helper: CP56Time2a_toString", "[remote::helper]") {
  sCP56Time2a time{.encodedValue = {0, 0, 0, 0, 0, 0, 0}};
  CP56Time2a_createFromMsTimestamp(&time, 1680517666000);
  auto s = Remote::CP56Time2a_toString(&time);
  REQUIRE(s.find(":") != std::string::npos);
}

TEST_CASE("Helper: connectionStringFormatter", "[remote::helper]") {
  REQUIRE(Remote::connectionStringFormatter("127.0.0.1", 2404) ==
          "127.0.0.1:2404");
  // port 0 -> default port
  REQUIRE(Remote::connectionStringFormatter("10.0.0.1", 0) == "10.0.0.1:2404");
}

TEST_CASE("Helper: file_exists", "[remote::helper]") {
  REQUIRE(Remote::file_exists("IEC60870-5-104.txt") == false);
  // a path that definitely exists (c104_tests runs from the build/covbuild
  // directory, so the repo-root tests folder is one level up)
  REQUIRE(Remote::file_exists("../tests/mock_helpers.h") == true);
}

TEST_CASE("Helper: rawMessageFormatter U-format branches", "[remote::helper]") {
  // U-format: FORMAT_OFFSET bit0 set + bit1 set
  for (uint32_t bit = 0x04; bit <= 0x80; bit <<= 1) {
    uint_fast8_t buf[12] = {0x68, 0x0A, 0x00, 0x00, 0x00, 0x00,
                            0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    buf[IEC60870_FORMAT_OFFSET] = static_cast<uint_fast8_t>(0x03 | bit);
    auto s = Remote::rawMessageFormatter(buf, 12);
    REQUIRE(s.find("U-Format") != std::string::npos);
  }
}

TEST_CASE("Helper: rawMessageFormatter S-format branch", "[remote::helper]") {
  uint_fast8_t buf[12] = {0x68, 0x0A, 0x00, 0x00, 0x00, 0x00,
                          0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
  buf[IEC60870_FORMAT_OFFSET] = 0x01; // bit0 set, bit1 clear => S-format
  auto s = Remote::rawMessageFormatter(buf, 12);
  REQUIRE(s.find("S-Format") != std::string::npos);
}

TEST_CASE("Helper: rawMessageFormatter I-format full parse",
          "[remote::helper]") {
  auto msg = makeIFrameBuffer(17);
  // COT positive, no test
  auto s =
      Remote::rawMessageFormatter(msg.data(), static_cast<int>(msg.size()));
  REQUIRE(s.find("I-Format") != std::string::npos);
  REQUIRE(s.find("M_SP_NA_1") != std::string::npos);
  REQUIRE(s.find("SEQUENCE[5]") != std::string::npos);
  REQUIRE(s.find("1st IOA 11") != std::string::npos);
  REQUIRE(s.find("| CA 47") != std::string::npos);
}

TEST_CASE("Helper: rawMessageFormatter I-format negative/test/global/oa",
          "[remote::helper]") {
  auto msg = makeIFrameBuffer(17);
  // negative + test flags
  msg[IEC60870_MSGINFO_OFFSET] = 0x40 | 0x80;
  // global common address + originator address
  msg[IEC60870_TARGETCA1_OFFSET] = 0xFF;
  msg[IEC60870_TARGETCA2_OFFSET] = 0xFF;
  msg[IEC60870_SOURCECA_OFFSET] = 0x05;
  // structure: no sequence
  msg[IEC60870_STRUCTURE_OFFSET] = 0x03;
  auto s =
      Remote::rawMessageFormatter(msg.data(), static_cast<int>(msg.size()));
  REQUIRE(s.find("I-Format") != std::string::npos);
  REQUIRE(s.find("NEGATIVE") != std::string::npos);
  REQUIRE(s.find("TEST") != std::string::npos);
  REQUIRE(s.find("GLOBAL") != std::string::npos);
  REQUIRE(s.find("| OA 5") != std::string::npos);
  REQUIRE(s.find("LIST[3]") != std::string::npos);
}

TEST_CASE("Helper: rawMessageFormatter APDU_LENGTH_EXCEEDED",
          "[remote::helper]") {
  // msgSize=20, length octet=13 => messageSize=15 (< msgSize) and > 12
  auto msg = makeIFrameBuffer(20);
  msg[1] = 13;
  auto s =
      Remote::rawMessageFormatter(msg.data(), static_cast<int>(msg.size()));
  REQUIRE(s.find("APDU_LENGTH_EXCEEDED") != std::string::npos);
}

TEST_CASE("Helper: rawMessageDictionaryFormatter U/S/I branches",
          "[remote::helper]") {
  // U-format
  {
    uint_fast8_t buf[12] = {0x68, 0x0A, 0x07, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto d = Remote::rawMessageDictionaryFormatter(buf, 12);
    REQUIRE(d["format"].cast<std::string>() == "U");
  }
  // S-format with rx
  {
    uint_fast8_t buf[12] = {0x68, 0x0A, 0x01, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    auto d = Remote::rawMessageDictionaryFormatter(buf, 12);
    REQUIRE(d["format"].cast<std::string>() == "S");
  }
  // I-format with full header
  {
    auto msg = makeIFrameBuffer(17);
    auto d = Remote::rawMessageDictionaryFormatter(
        msg.data(), static_cast<int>(msg.size()));
    REQUIRE(d["format"].cast<std::string>() == "I");
    REQUIRE(d.contains("type"));
    REQUIRE(d.contains("numberOfObjects"));
    REQUIRE(d.contains("sequence"));
    REQUIRE(d.contains("cot"));
    REQUIRE(d.contains("commonAddress"));
    REQUIRE(d.contains("firstInformationObjectAddress"));
  }
}

TEST_CASE("Helper: rawMessageDictionaryFormatter select flag & negative/test",
          "[remote::helper]") {
  // need messageSize > OBJECT_OFFSET+3 (=15) and a control type
  auto msg = makeIFrameBuffer(20);
  // control type C_SC_NA_1
  msg[IEC60870_TYPEID_OFFSET] =
      static_cast<uint8_t>(IEC60870_5_TypeID::C_SC_NA_1);
  msg[IEC60870_OBJECT_OFFSET + 3] = 0x80; // select bit
  auto d = Remote::rawMessageDictionaryFormatter(msg.data(),
                                                 static_cast<int>(msg.size()));
  REQUIRE(d.contains("select"));
  REQUIRE(d["select"].cast<bool>() == true);
  REQUIRE(d.contains("elements"));
}

// ===========================================================================
// Intra-station interactions (Server helpers used by Station/DataPoint)
// ===========================================================================

TEST_CASE("Station/DataPoint: Server::getTickRate forwarding & multiple points",
          "[object::point]") {
  auto server = Server::create("127.0.0.1", 2404, 200);
  REQUIRE(server->getTickRate_ms() == 200);
  auto station = server->addStation(5);
  // 200 is a valid multiple so report interval of 200 works
  auto p = station->addPoint(1, IEC60870_5_TypeID::M_SP_NA_1, 200);
  REQUIRE(p->getReportInterval_ms() == 200);
}
