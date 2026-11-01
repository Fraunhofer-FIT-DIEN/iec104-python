/**
 * Copyright 2026 Fraunhofer Institute for Applied Information Technology
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
 * @file test_enums_types.cpp
 * @brief Catch2 coverage tests for src/enums.cpp, src/types.cpp and
 * src/numbers.h. Drives every toString() switch branch (including the UNKNOWN
 * / unsupported-bits fallbacks) of the enum converters, the bitmask helpers
 * (is_any/is_none/test/any_of/reset and the bitwise operators), the
 * fixed-width integer wrappers and the value / debug helpers in types.cpp.
 *
 * NOTE ON GIL: no second interpreter is created here; tests/main.cpp keeps one
 * alive for the whole process. We still include <pybind11/pybind11.h> (as the
 * other test TUs do) so any GIL-touching code path stays safe. types.cpp,
 * enums.cpp and numbers.h contain no GIL-touching code themselves.
 */

#include <catch2/catch_approx.hpp>
#include <catch2/catch_template_test_macros.hpp>
#include <catch2/catch_test_macros.hpp>

#include <pybind11/pybind11.h>

#include <chrono>
#include <sstream>
#include <stdexcept>

#include "enums.h"
#include "numbers.h"
#include "types.h"

using Catch::Approx;

using namespace std::chrono_literals;

// ---------------------------------------------------------------------------
// src/numbers.h -- fixed-width integer wrappers
// ---------------------------------------------------------------------------
TEST_CASE("LimitedInteger base ctor throws", "[numbers]") {
  // The base LimitedInteger<T> ctor always throws std::domain_error.
  REQUIRE_THROWS_AS(LimitedInteger<int>(5), std::domain_error);
}

TEMPLATE_TEST_CASE("Limited integer wrappers round-trip and bounds",
                   "[numbers]", LimitedUInt5, LimitedUInt7, LimitedUInt16,
                   LimitedInt7, LimitedInt16) {
  TestType v(14);
  REQUIRE(v.get() == 14);
  v.set(30);
  REQUIRE(v.get() == 30);
  // default constructed -> 0
  TestType d;
  REQUIRE(d.get() == 0);
  // range sanity
  REQUIRE(v.getMin() <= v.getMax());
  // out of range on construction
  REQUIRE_THROWS_AS(TestType(v.getMax() + 1000000), std::overflow_error);
  // out of range on set (above max and below min)
  REQUIRE_THROWS_AS(v.set(v.getMax() + 1000000), std::overflow_error);
  REQUIRE_THROWS_AS(v.set(v.getMin() - 1000000), std::overflow_error);
}

TEMPLATE_TEST_CASE("Limited integer arithmetic and conversions", "[numbers]",
                   LimitedUInt5, LimitedUInt7, LimitedUInt16, LimitedInt7,
                   LimitedInt16) {
  TestType v(5);
  // arithmetic returning plain int
  REQUIRE(v + 2 == 7);
  REQUIRE(v - 2 == 3);
  REQUIRE(v * 2 == 10);
  REQUIRE(v / 2 == 2);
  // compound assignment returns wrapped type, then get()
  v += 3;
  REQUIRE(v.get() == 8);
  v -= 3;
  REQUIRE(v.get() == 5);
  v *= 2;
  REQUIRE(v.get() == 10);
  v /= 2;
  REQUIRE(v.get() == 5);
  // division by zero throws
  REQUIRE_THROWS_AS(v / 0, std::runtime_error);
  REQUIRE_THROWS_AS(v /= 0, std::runtime_error);
  // compound out-of-range throws
  REQUIRE_THROWS_AS(v += 1000000, std::overflow_error);
  REQUIRE_THROWS_AS(v -= 1000000, std::overflow_error);
  REQUIRE_THROWS_AS(v *= 1000000, std::overflow_error);
}

TEST_CASE("LimitedUInt16 max boundary", "[numbers]") {
  LimitedUInt16 v(65535);
  REQUIRE(v.get() == 65535);
  REQUIRE(v.getMax() == 65535);
  REQUIRE_THROWS_AS(LimitedUInt16(65536), std::overflow_error);
}

TEST_CASE("LimitedInt16 min boundary", "[numbers]") {
  LimitedInt16 v(-32768);
  REQUIRE(v.get() == -32768);
  REQUIRE(v.getMin() == -32768);
  REQUIRE_THROWS_AS(LimitedInt16(-32769), std::overflow_error);
}

TEST_CASE("NormalizedFloat get/set and bounds", "[numbers]") {
  NormalizedFloat nf;
  REQUIRE(Approx(nf.get()) == 0.0f);
  REQUIRE(nf.getMin() == -1.f);
  REQUIRE(nf.getMax() == 1.f);
  nf.set(0.5f);
  REQUIRE(Approx(nf.get()) == 0.5f);
  nf.set(-1.0f);
  REQUIRE(Approx(nf.get()) == -1.0f);
  nf.set(1.0f);
  REQUIRE(Approx(nf.get()) == 1.0f);
  // int ctor
  NormalizedFloat nfi(1);
  REQUIRE(Approx(nfi.get()) == 1.0f);
  // out of range
  REQUIRE_THROWS_AS(NormalizedFloat(2.0f), std::overflow_error);
  REQUIRE_THROWS_AS(NormalizedFloat(-2.0f), std::overflow_error);
  REQUIRE_THROWS_AS(NormalizedFloat(2), std::overflow_error);
  REQUIRE_THROWS_AS(nf.set(2.0f), std::overflow_error);
  REQUIRE_THROWS_AS(nf.set(-2.0f), std::overflow_error);
}

TEST_CASE("NormalizedFloat int arithmetic", "[numbers]") {
  NormalizedFloat nf(0.25f);
  // Non-compound operators return a raw result without enforcing [-1, 1]
  REQUIRE(Approx(nf + 1) == 1.25f);
  REQUIRE(Approx(nf - 1) == -0.75f);
  REQUIRE(Approx(nf * 2) == 0.5f);
  REQUIRE(Approx(nf / 2) == 0.125f);
  // Compound assignment operators enforce the [-1, 1] range
  nf *= 2;
  REQUIRE(Approx(nf.get()) == 0.5f);
  nf *= 2;
  REQUIRE(Approx(nf.get()) == 1.0f);
  nf /= 2;
  REQUIRE(Approx(nf.get()) == 0.5f);
  REQUIRE_THROWS_AS(nf / 0, std::runtime_error);
  REQUIRE_THROWS_AS(nf /= 0, std::runtime_error);
  // Compound arithmetic leaving the range throws
  REQUIRE_THROWS_AS(nf += 1, std::overflow_error);
  REQUIRE_THROWS_AS(nf += 1000000, std::overflow_error);
}

TEST_CASE("NormalizedFloat float arithmetic", "[numbers]") {
  NormalizedFloat nf(0.25f);
  // Non-compound operators return a raw result without enforcing [-1, 1]
  REQUIRE(Approx(nf + 1.0f) == 1.25f);
  REQUIRE(Approx(nf - 1.0f) == -0.75f);
  REQUIRE(Approx(nf * 2.0f) == 0.5f);
  REQUIRE(Approx(nf / 2.0f) == 0.125f);
  // Compound assignment operators enforce the [-1, 1] range
  nf *= 2.0f;
  REQUIRE(Approx(nf.get()) == 0.5f);
  nf *= 2.0f;
  REQUIRE(Approx(nf.get()) == 1.0f);
  nf /= 2.0f;
  REQUIRE(Approx(nf.get()) == 0.5f);
  REQUIRE_THROWS_AS(nf / 0.0f, std::runtime_error);
  REQUIRE_THROWS_AS(nf /= 0.0f, std::runtime_error);
  // Compound arithmetic leaving the range throws
  REQUIRE_THROWS_AS(nf += 1.0f, std::overflow_error);
  REQUIRE_THROWS_AS(nf += 1000000.0f, std::overflow_error);
}

TEST_CASE("Byte32 get/set", "[numbers]") {
  Byte32 b;
  REQUIRE(b.get() == 0);
  b.set(42);
  REQUIRE(b.get() == 42);
  Byte32 b2(123456789u);
  REQUIRE(b2.get() == 123456789u);
}

// The Python bindings back equality (__eq__/__ne__) and the .value property
// with get(). These tests pin that semantics at the C++ level: equal-valued
// instances compare equal and .value reflects get().
TEMPLATE_TEST_CASE("Limited integer equality and .value backing", "[numbers]",
                   LimitedUInt5, LimitedUInt7, LimitedUInt16, LimitedInt7,
                   LimitedInt16) {
  TestType a(5);
  TestType b(5);
  TestType c(6);
  // equal-valued instances compare equal / unequal
  REQUIRE(a.get() == b.get());
  REQUIRE(a.get() != c.get());
  // the .value getter returns get()
  REQUIRE(static_cast<int>(a.get()) == 5);
  REQUIRE(static_cast<int>(c.get()) == 6);
}

TEST_CASE("Byte32 little-endian bytes round-trip", "[numbers]") {
  // The IEC 60870-5-4 protocol wire format (and bytes()) puts the least
  // significant byte first (little-endian). Replicate the
  // __bytes__/Byte32(py::bytes) conversion exactly as implemented in
  // python.cpp.
  Byte32 b(0x11223344u);
  uint32_t value = b.get();
  uint8_t wire[4] = {static_cast<uint8_t>(value & 0xFF),
                     static_cast<uint8_t>((value >> 8) & 0xFF),
                     static_cast<uint8_t>((value >> 16) & 0xFF),
                     static_cast<uint8_t>((value >> 24) & 0xFF)};
  REQUIRE(wire[0] == 0x44);
  REQUIRE(wire[1] == 0x33);
  REQUIRE(wire[2] == 0x22);
  REQUIRE(wire[3] == 0x11);

  // decoding b"\x44\x33\x22\x11" back to a value (little-endian)
  const uint8_t src[4] = {0x44, 0x33, 0x22, 0x11};
  uint32_t decoded = static_cast<uint32_t>(src[0]) |
                     (static_cast<uint32_t>(src[1]) << 8) |
                     (static_cast<uint32_t>(src[2]) << 16) |
                     (static_cast<uint32_t>(src[3]) << 24);
  REQUIRE(decoded == 0x11223344u);
  REQUIRE(Byte32(decoded).get() == 0x11223344u);

  // bytes()<->Byte32 round-trip preserves the value
  Byte32 rt(0xDEADBEEFu);
  uint32_t rv = rt.get();
  uint8_t renc[4] = {static_cast<uint8_t>(rv & 0xFF),
                     static_cast<uint8_t>((rv >> 8) & 0xFF),
                     static_cast<uint8_t>((rv >> 16) & 0xFF),
                     static_cast<uint8_t>((rv >> 24) & 0xFF)};
  uint32_t rdec = static_cast<uint32_t>(renc[0]) |
                  (static_cast<uint32_t>(renc[1]) << 8) |
                  (static_cast<uint32_t>(renc[2]) << 16) |
                  (static_cast<uint32_t>(renc[3]) << 24);
  REQUIRE(Byte32(rdec).get() == 0xDEADBEEFu);
}

TEST_CASE("NormalizedFloat arithmetic with NormalizedFloat", "[numbers]") {
  NormalizedFloat a(0.25f);
  NormalizedFloat b(0.5f);

  // Non-compound operators return a NormalizedFloat and enforce [-1, 1]
  // (0.25 + 0.5) == 0.75
  REQUIRE(Approx((a + b).get()) == 0.75f);
  REQUIRE(Approx((b - a).get()) == 0.25f);
  REQUIRE(Approx((a * b).get()) == 0.125f);
  REQUIRE(Approx((a / b).get()) == 0.5f);

  // results leaving [-1, 1] throw
  REQUIRE_THROWS_AS(a + b + b, std::overflow_error); // 0.25+0.5+0.5 = 1.25
  REQUIRE_THROWS_AS(b / a, std::overflow_error);     // 0.5 / 0.25 = 2.0
  // 0.25 - 0.5 - 0.5 - 0.5 = -1.25
  REQUIRE_THROWS_AS(a - b - b - b, std::overflow_error);

  // division by zero throws
  REQUIRE_THROWS_AS(a / NormalizedFloat(0.0f), std::runtime_error);

  // compound assignment is range-guarded and returns the wrapped type
  a += b; // 0.75
  REQUIRE(Approx(a.get()) == 0.75f);
  a -= b; // 0.25
  REQUIRE(Approx(a.get()) == 0.25f);
  a *= b; // 0.125
  REQUIRE(Approx(a.get()) == 0.125f);
  a /= b; // 0.25
  REQUIRE(Approx(a.get()) == 0.25f);
  REQUIRE_THROWS_AS(a += b + b, std::overflow_error); // 0.25+0.5+0.5 = 1.25
  REQUIRE_THROWS_AS(a /= NormalizedFloat(0.0f), std::runtime_error);
}

// ---------------------------------------------------------------------------
// src/enums.cpp -- bitmask helpers (templates in enums.h)
// ---------------------------------------------------------------------------
TEST_CASE("Quality bitmask helpers", "[enums]") {
  Quality q = Quality::None;
  REQUIRE(is_none(q));
  REQUIRE_FALSE(is_any(q));
  // test() requires non-zero operands
  REQUIRE_FALSE(test(q, Quality::Overflow));
  REQUIRE_FALSE(any_of(q, Quality::Overflow));

  q = Quality::Overflow | Quality::Invalid;
  REQUIRE(is_any(q));
  REQUIRE_FALSE(is_none(q));
  REQUIRE(test(q, Quality::Overflow));
  REQUIRE(test(q, Quality::Invalid));
  REQUIRE(test(q, Quality::Overflow | Quality::Invalid));
  REQUIRE_FALSE(test(q, Quality::Blocked));
  REQUIRE_FALSE(any_of(q, Quality::Blocked));
  REQUIRE(any_of(q, Quality::Overflow));

  // bitwise ops
  Quality a = Quality::Overflow;
  Quality b = Quality::Invalid;
  REQUIRE((a | b) == (Quality::Overflow | Quality::Invalid));
  REQUIRE((a & (a | b)) == Quality::Overflow);
  REQUIRE(((a ^ b) & a) == Quality::Overflow);
  REQUIRE(((~q) & q) == Quality::None);
  // compound assignment
  Quality c = Quality::None;
  c |= Quality::Overflow;
  REQUIRE(c == Quality::Overflow);
  c &= Quality::Overflow;
  REQUIRE(c == Quality::Overflow);
  c ^= Quality::Overflow;
  REQUIRE(c == Quality::None);
  // reset
  Quality r = Quality::Invalid;
  reset(r);
  REQUIRE(r == Quality::None);
  REQUIRE(is_none(r));

  // operator<< outputs the underlying integer value
  std::ostringstream oss;
  oss << Quality::Overflow;
  REQUIRE(oss.str() == "1");
}

// ---------------------------------------------------------------------------
// src/enums.cpp -- simple enum toString sweeps (with UNKNOWN default branch)
// ---------------------------------------------------------------------------
TEST_CASE("QualifierOfCommand_toString", "[enums]") {
  REQUIRE(QualifierOfCommand_toString(CS101_QualifierOfCommand::NONE) ==
          "NONE");
  REQUIRE(QualifierOfCommand_toString(CS101_QualifierOfCommand::SHORT_PULSE) ==
          "SHORT_PULSE");
  REQUIRE(QualifierOfCommand_toString(CS101_QualifierOfCommand::LONG_PULSE) ==
          "LONG_PULSE");
  REQUIRE(QualifierOfCommand_toString(CS101_QualifierOfCommand::PERSISTENT) ==
          "PERSISTENT");
  REQUIRE(QualifierOfCommand_toString(
              static_cast<CS101_QualifierOfCommand>(99)) == "UNKNOWN");
}

TEST_CASE("ConnectionState_toString", "[enums]") {
  REQUIRE(ConnectionState_toString(CLOSED) == "CLOSED");
  REQUIRE(ConnectionState_toString(CLOSED_AWAIT_OPEN) == "CLOSED_AWAIT_OPEN");
  REQUIRE(ConnectionState_toString(CLOSED_AWAIT_RECONNECT) ==
          "CLOSED_AWAIT_RECONNECT");
  REQUIRE(ConnectionState_toString(OPEN_MUTED) == "OPEN_MUTED");
  REQUIRE(ConnectionState_toString(OPEN) == "OPEN");
  REQUIRE(ConnectionState_toString(OPEN_AWAIT_CLOSED) == "OPEN_AWAIT_CLOSED");
  REQUIRE(ConnectionState_toString(static_cast<ConnectionState>(99)) ==
          "UNKNOWN");
}

TEST_CASE("ConnectionEvent_toString", "[enums]") {
  REQUIRE(ConnectionEvent_toString(CS104_CONNECTION_OPENED) == "OPENED");
  REQUIRE(ConnectionEvent_toString(CS104_CONNECTION_CLOSED) == "CLOSED");
  REQUIRE(ConnectionEvent_toString(CS104_CONNECTION_STARTDT_CON_RECEIVED) ==
          "ACTIVATED");
  REQUIRE(ConnectionEvent_toString(CS104_CONNECTION_STOPDT_CON_RECEIVED) ==
          "DEACTIVATED");
  REQUIRE(ConnectionEvent_toString(CS104_CONNECTION_FAILED) == "FAILED");
  REQUIRE(ConnectionEvent_toString(static_cast<CS104_ConnectionEvent>(99)) ==
          "UNKNOWN");
}

TEST_CASE("PeerConnectionEvent_toString", "[enums]") {
  REQUIRE(PeerConnectionEvent_toString(CS104_CON_EVENT_CONNECTION_OPENED) ==
          "OPENED");
  REQUIRE(PeerConnectionEvent_toString(CS104_CON_EVENT_CONNECTION_CLOSED) ==
          "CLOSED");
  REQUIRE(PeerConnectionEvent_toString(CS104_CON_EVENT_ACTIVATED) ==
          "ACTIVATED");
  REQUIRE(PeerConnectionEvent_toString(CS104_CON_EVENT_DEACTIVATED) ==
          "DEACTIVATED");
  REQUIRE(PeerConnectionEvent_toString(
              static_cast<CS104_PeerConnectionEvent>(99)) == "UNKNOWN");
}

TEST_CASE("DoublePointValue_toString", "[enums]") {
  REQUIRE(DoublePointValue_toString(IEC60870_DOUBLE_POINT_INDETERMINATE) ==
          "INDETERMINATE");
  REQUIRE(DoublePointValue_toString(IEC60870_DOUBLE_POINT_OFF) == "OFF");
  REQUIRE(DoublePointValue_toString(IEC60870_DOUBLE_POINT_ON) == "ON");
  REQUIRE(DoublePointValue_toString(IEC60870_DOUBLE_POINT_INTERMEDIATE) ==
          "INTERMEDIATE");
  REQUIRE(DoublePointValue_toString(static_cast<DoublePointValue>(99)) ==
          "UNKNOWN");
}

TEST_CASE("StepCommandValue_toString", "[enums]") {
  REQUIRE(StepCommandValue_toString(IEC60870_STEP_INVALID_0) == "INVALID_0");
  REQUIRE(StepCommandValue_toString(IEC60870_STEP_LOWER) == "LOWER");
  REQUIRE(StepCommandValue_toString(IEC60870_STEP_HIGHER) == "HIGHER");
  REQUIRE(StepCommandValue_toString(IEC60870_STEP_INVALID_3) == "INVALID_3");
  REQUIRE(StepCommandValue_toString(static_cast<StepCommandValue>(99)) ==
          "UNKNOWN");
}

TEST_CASE("EventState_toString", "[enums]") {
  REQUIRE(EventState_toString(IEC60870_EVENTSTATE_INDETERMINATE_0) ==
          "INDETERMINATE_0");
  REQUIRE(EventState_toString(IEC60870_EVENTSTATE_OFF) == "OFF");
  REQUIRE(EventState_toString(IEC60870_EVENTSTATE_ON) == "ON");
  REQUIRE(EventState_toString(IEC60870_EVENTSTATE_INDETERMINATE_3) ==
          "INDETERMINATE_3");
  REQUIRE(EventState_toString(static_cast<EventState>(99)) == "UNKNOWN");
}

TEST_CASE("CommandTransmissionMode_toString", "[enums]") {
  REQUIRE(CommandTransmissionMode_toString(DIRECT_COMMAND) == "DIRECT");
  REQUIRE(CommandTransmissionMode_toString(SELECT_AND_EXECUTE_COMMAND) ==
          "SELECT_AND_EXECUTE");
  REQUIRE(CommandTransmissionMode_toString(
              static_cast<CommandTransmissionMode>(99)) == "UNKNOWN");
}

TEST_CASE("UnexpectedMessageCause_toString", "[enums]") {
  REQUIRE(UnexpectedMessageCause_toString(NO_ERROR_CAUSE) == "NO_ERROR_CAUSE");
  REQUIRE(UnexpectedMessageCause_toString(UNKNOWN_TYPE_ID) ==
          "UNKNOWN_TYPE_ID");
  REQUIRE(UnexpectedMessageCause_toString(UNKNOWN_COT) == "UNKNOWN_COT");
  REQUIRE(UnexpectedMessageCause_toString(UNKNOWN_CA) == "UNKNOWN_CA");
  REQUIRE(UnexpectedMessageCause_toString(UNKNOWN_IOA) == "UNKNOWN_IOA");
  REQUIRE(UnexpectedMessageCause_toString(INVALID_COT) == "INVALID_COT");
  REQUIRE(UnexpectedMessageCause_toString(INVALID_TYPE_ID) ==
          "INVALID_TYPE_ID");
  REQUIRE(UnexpectedMessageCause_toString(MISMATCHED_TYPE_ID) ==
          "MISMATCHED_TYPE_ID");
  REQUIRE(UnexpectedMessageCause_toString(UNIMPLEMENTED_GROUP) ==
          "UNIMPLEMENTED_GROUP");
  REQUIRE(UnexpectedMessageCause_toString(
              static_cast<UnexpectedMessageCause>(99)) == "UNKNOWN");
}

TEST_CASE("CauseOfInitialization_toString", "[enums]") {
  REQUIRE(CauseOfInitialization_toString(
              CS101_CauseOfInitialization::LOCAL_POWER_ON) == "LOCAL_POWER_ON");
  REQUIRE(CauseOfInitialization_toString(
              CS101_CauseOfInitialization::LOCAL_MANUAL_RESET) ==
          "LOCAL_MANUAL_RESET");
  REQUIRE(CauseOfInitialization_toString(
              CS101_CauseOfInitialization::REMOTE_RESET) == "REMOTE_RESET");
  REQUIRE(CauseOfInitialization_toString(
              static_cast<CS101_CauseOfInitialization>(99)) == "UNKNOWN");
}

TEST_CASE("ConnectionInit_toString", "[enums]") {
  REQUIRE(ConnectionInit_toString(ConnectionInit::INIT_ALL) == "INIT_ALL");
  REQUIRE(ConnectionInit_toString(ConnectionInit::INIT_INTERROGATION) ==
          "INIT_INTERROGATION");
  REQUIRE(ConnectionInit_toString(ConnectionInit::INIT_CLOCK_SYNC) ==
          "INIT_CLOCK_SYNC");
  REQUIRE(ConnectionInit_toString(ConnectionInit::INIT_MUTED) == "INIT_MUTED");
  REQUIRE(ConnectionInit_toString(ConnectionInit::INIT_NONE) == "INIT_NONE");
  REQUIRE(ConnectionInit_toString(static_cast<ConnectionInit>(99)) ==
          "UNKNOWN");
}

TEST_CASE("CommandResponseState_toString", "[enums]") {
  REQUIRE(CommandResponseState_toString(RESPONSE_STATE_FAILURE) ==
          "RESPONSE_STATE_FAILURE");
  REQUIRE(CommandResponseState_toString(RESPONSE_STATE_SUCCESS) ==
          "RESPONSE_STATE_SUCCESS");
  REQUIRE(CommandResponseState_toString(RESPONSE_STATE_NONE) ==
          "RESPONSE_STATE_NONE");
  REQUIRE(CommandResponseState_toString(
              static_cast<CommandResponseState>(99)) == "UNKNOWN");
}

TEST_CASE("CommandProcessState_toString", "[enums]") {
  REQUIRE(CommandProcessState_toString(COMMAND_FAILURE) == "COMMAND_FAILURE");
  REQUIRE(CommandProcessState_toString(COMMAND_SUCCESS) == "COMMAND_SUCCESS");
  REQUIRE(CommandProcessState_toString(COMMAND_AWAIT_CON) ==
          "COMMAND_AWAIT_CON");
  REQUIRE(CommandProcessState_toString(COMMAND_AWAIT_TERM) ==
          "COMMAND_AWAIT_TERM");
  REQUIRE(CommandProcessState_toString(COMMAND_AWAIT_CON_TERM) ==
          "COMMAND_AWAIT_CON_TERM");
  REQUIRE(CommandProcessState_toString(COMMAND_AWAIT_REQUEST) ==
          "COMMAND_AWAIT_REQUEST");
  REQUIRE(CommandProcessState_toString(static_cast<CommandProcessState>(99)) ==
          "UNKNOWN");
}

// ---------------------------------------------------------------------------
// src/enums.cpp -- bitmask enum toString sweeps
// ---------------------------------------------------------------------------
TEST_CASE("Debug_toString and Debug_toFlagString", "[enums]") {
  REQUIRE(Debug_toString(Debug::None) == "Debug set: {}, is_none: True");
  REQUIRE(Debug_toFlagString(Debug::None) == "None");
  // every single bit
  REQUIRE(Debug_toFlagString(Debug::Server) == "Server");
  REQUIRE(Debug_toFlagString(Debug::Client) == "Client");
  REQUIRE(Debug_toFlagString(Debug::Connection) == "Connection");
  REQUIRE(Debug_toFlagString(Debug::Station) == "Station");
  REQUIRE(Debug_toFlagString(Debug::Point) == "Point");
  REQUIRE(Debug_toFlagString(Debug::Message) == "Message");
  REQUIRE(Debug_toFlagString(Debug::Callback) == "Callback");
  REQUIRE(Debug_toFlagString(Debug::Gil) == "Gil");
  // combined
  auto combo = Debug::Server | Debug::Client | Debug::Gil;
  REQUIRE(Debug_toFlagString(combo) == "Server | Client | Gil");
  REQUIRE(Debug_toString(combo) ==
          "Debug set: { Server | Client | Gil }, is_none: False");
  // All has every bit
  auto all_flags = std::string(Debug_toFlagString(Debug::All));
  REQUIRE(all_flags.find("Server") != std::string::npos);
  REQUIRE(all_flags.find("Client") != std::string::npos);
  REQUIRE(all_flags.find("Connection") != std::string::npos);
  REQUIRE(all_flags.find("Station") != std::string::npos);
  REQUIRE(all_flags.find("Point") != std::string::npos);
  REQUIRE(all_flags.find("Message") != std::string::npos);
  REQUIRE(all_flags.find("Callback") != std::string::npos);
  REQUIRE(all_flags.find("Gil") != std::string::npos);
  REQUIRE(Debug_toString(Debug::All).find(" | ") != std::string::npos);
}

TEST_CASE("Quality_toString", "[enums]") {
  REQUIRE(Quality_toString(Quality::None) == "Quality set: {}, is_good: True");
  REQUIRE(Quality_toString(Quality::Overflow) ==
          "Quality set: { Overflow }, is_good: False");
  REQUIRE(Quality_toString(Quality::ElapsedTimeInvalid) ==
          "Quality set: { ElapsedTimeInvalid }, is_good: False");
  REQUIRE(Quality_toString(Quality::Blocked) ==
          "Quality set: { Blocked }, is_good: False");
  REQUIRE(Quality_toString(Quality::Substituted) ==
          "Quality set: { Substituted }, is_good: False");
  REQUIRE(Quality_toString(Quality::NonTopical) ==
          "Quality set: { NonTopical }, is_good: False");
  REQUIRE(Quality_toString(Quality::Invalid) ==
          "Quality set: { Invalid }, is_good: False");
  // combined
  auto c = Quality::Overflow | Quality::Invalid;
  REQUIRE(Quality_toString(c) ==
          "Quality set: { Overflow | Invalid }, is_good: False");
  // unsupported bits
  REQUIRE(Quality_toString(static_cast<Quality>(0x02)) ==
          "Quality set: { UNSUPPORTED_BIT_1 }, is_good: False");
  REQUIRE(Quality_toString(static_cast<Quality>(0x04)) ==
          "Quality set: { UNSUPPORTED_BIT_2 }, is_good: False");
  REQUIRE(Quality_toString(static_cast<Quality>(0x02 | 0x04))
              .find("UNSUPPORTED_BIT_1 | UNSUPPORTED_BIT_2") !=
          std::string::npos);
}

TEST_CASE("BinaryCounterQuality_toString", "[enums]") {
  REQUIRE(BinaryCounterQuality_toString(BinaryCounterQuality::None) ==
          "BinaryCounterQuality set: {}, is_good: True");
  REQUIRE(BinaryCounterQuality_toString(BinaryCounterQuality::Adjusted) ==
          "BinaryCounterQuality set: { Adjusted }, is_good: False");
  REQUIRE(BinaryCounterQuality_toString(BinaryCounterQuality::Carry) ==
          "BinaryCounterQuality set: { Carry }, is_good: False");
  REQUIRE(BinaryCounterQuality_toString(BinaryCounterQuality::Invalid) ==
          "BinaryCounterQuality set: { Invalid }, is_good: False");
  // each unsupported bit
  REQUIRE(BinaryCounterQuality_toString(static_cast<BinaryCounterQuality>(0x01))
              .find("UNSUPPORTED_BIT_0") != std::string::npos);
  REQUIRE(BinaryCounterQuality_toString(static_cast<BinaryCounterQuality>(0x02))
              .find("UNSUPPORTED_BIT_1") != std::string::npos);
  REQUIRE(BinaryCounterQuality_toString(static_cast<BinaryCounterQuality>(0x04))
              .find("UNSUPPORTED_BIT_2") != std::string::npos);
  REQUIRE(BinaryCounterQuality_toString(static_cast<BinaryCounterQuality>(0x08))
              .find("UNSUPPORTED_BIT_3") != std::string::npos);
  REQUIRE(BinaryCounterQuality_toString(static_cast<BinaryCounterQuality>(0x10))
              .find("UNSUPPORTED_BIT_4") != std::string::npos);
  // combined
  auto c = BinaryCounterQuality::Adjusted | BinaryCounterQuality::Invalid;
  REQUIRE(BinaryCounterQuality_toString(c) ==
          "BinaryCounterQuality set: { Adjusted | Invalid }, is_good: False");
}

TEST_CASE("StartEvents_toString", "[enums]") {
  REQUIRE(StartEvents_toString(StartEvents::None) == "StartEvent set: {}");
  REQUIRE(StartEvents_toString(StartEvents::General) ==
          "StartEvents set: { General }");
  REQUIRE(StartEvents_toString(StartEvents::PhaseL1) ==
          "StartEvents set: { PhaseL1 }");
  REQUIRE(StartEvents_toString(StartEvents::PhaseL2) ==
          "StartEvents set: { PhaseL2 }");
  REQUIRE(StartEvents_toString(StartEvents::PhaseL3) ==
          "StartEvents set: { PhaseL3 }");
  REQUIRE(StartEvents_toString(StartEvents::InEarthCurrent) ==
          "StartEvents set: { InEarthCurrent }");
  REQUIRE(StartEvents_toString(StartEvents::ReverseDirection) ==
          "StartEvents set: { ReverseDirection }");
  // unsupported bits
  REQUIRE(StartEvents_toString(static_cast<StartEvents>(0x40))
              .find("UNSUPPORTED_BIT_6") != std::string::npos);
  REQUIRE(StartEvents_toString(static_cast<StartEvents>(0x80))
              .find("UNSUPPORTED_BIT_7") != std::string::npos);
  // combined
  auto c = StartEvents::General | StartEvents::PhaseL1;
  REQUIRE(StartEvents_toString(c) == "StartEvents set: { General | PhaseL1 }");
}

TEST_CASE("OutputCircuits_toString", "[enums]") {
  REQUIRE(OutputCircuits_toString(OutputCircuits::None) ==
          "OutputCircuit set: {}");
  REQUIRE(OutputCircuits_toString(OutputCircuits::General) ==
          "OutputCircuit set: { General }");
  REQUIRE(OutputCircuits_toString(OutputCircuits::PhaseL1) ==
          "OutputCircuit set: { PhaseL1 }");
  REQUIRE(OutputCircuits_toString(OutputCircuits::PhaseL2) ==
          "OutputCircuit set: { PhaseL2 }");
  REQUIRE(OutputCircuits_toString(OutputCircuits::PhaseL3) ==
          "OutputCircuit set: { PhaseL3 }");
  REQUIRE(OutputCircuits_toString(static_cast<OutputCircuits>(0x10))
              .find("UNSUPPORTED_BIT_4") != std::string::npos);
  REQUIRE(OutputCircuits_toString(static_cast<OutputCircuits>(0x20))
              .find("UNSUPPORTED_BIT_5") != std::string::npos);
  REQUIRE(OutputCircuits_toString(static_cast<OutputCircuits>(0x40))
              .find("UNSUPPORTED_BIT_6") != std::string::npos);
  REQUIRE(OutputCircuits_toString(static_cast<OutputCircuits>(0x80))
              .find("UNSUPPORTED_BIT_7") != std::string::npos);
  auto c = OutputCircuits::PhaseL1 | OutputCircuits::PhaseL2;
  REQUIRE(OutputCircuits_toString(c) ==
          "OutputCircuit set: { PhaseL1 | PhaseL2 }");
}

TEST_CASE("FieldSet16_toString", "[enums]") {
  REQUIRE(FieldSet16_toString(FieldSet16::None) == "Field set: {}");
  // all 16 bits
  REQUIRE(FieldSet16_toString(FieldSet16::I0) == "PackedSingle set: { I0 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I1) == "PackedSingle set: { I1 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I2) == "PackedSingle set: { I2 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I3) == "PackedSingle set: { I3 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I4) == "PackedSingle set: { I4 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I5) == "PackedSingle set: { I5 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I6) == "PackedSingle set: { I6 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I7) == "PackedSingle set: { I7 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I8) == "PackedSingle set: { I8 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I9) == "PackedSingle set: { I9 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I10) == "PackedSingle set: { I10 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I11) == "PackedSingle set: { I11 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I12) == "PackedSingle set: { I12 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I13) == "PackedSingle set: { I13 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I14) == "PackedSingle set: { I14 }");
  REQUIRE(FieldSet16_toString(FieldSet16::I15) == "PackedSingle set: { I15 }");
  auto c = FieldSet16::I0 | FieldSet16::I1 | FieldSet16::I15;
  REQUIRE(FieldSet16_toString(c) == "PackedSingle set: { I0 | I1 | I15 }");
}

// ---------------------------------------------------------------------------
// src/enums.cpp -- TLSCipherSuite (190 members) full sweep + UNKNOWN
// ---------------------------------------------------------------------------
TEST_CASE("TLSCipherSuite_toString full sweep", "[enums]") {
  const std::pair<TLSCipherSuite, const char *> table[] = {
      {TLSCipherSuite::RSA_WITH_NULL_MD5, "RSA_WITH_NULL_MD5"},
      {TLSCipherSuite::RSA_WITH_NULL_SHA, "RSA_WITH_NULL_SHA"},
      {TLSCipherSuite::PSK_WITH_NULL_SHA, "PSK_WITH_NULL_SHA"},
      {TLSCipherSuite::DHE_PSK_WITH_NULL_SHA, "DHE_PSK_WITH_NULL_SHA"},
      {TLSCipherSuite::RSA_PSK_WITH_NULL_SHA, "RSA_PSK_WITH_NULL_SHA"},
      {TLSCipherSuite::RSA_WITH_AES_128_CBC_SHA, "RSA_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_128_CBC_SHA,
       "DHE_RSA_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::RSA_WITH_AES_256_CBC_SHA, "RSA_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_256_CBC_SHA,
       "DHE_RSA_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::RSA_WITH_NULL_SHA256, "RSA_WITH_NULL_SHA256"},
      {TLSCipherSuite::RSA_WITH_AES_128_CBC_SHA256,
       "RSA_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::RSA_WITH_AES_256_CBC_SHA256,
       "RSA_WITH_AES_256_CBC_SHA256"},
      {TLSCipherSuite::RSA_WITH_CAMELLIA_128_CBC_SHA,
       "RSA_WITH_CAMELLIA_128_CBC_SHA"},
      {TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
       "DHE_RSA_WITH_CAMELLIA_128_CBC_SHA"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_128_CBC_SHA256,
       "DHE_RSA_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_256_CBC_SHA256,
       "DHE_RSA_WITH_AES_256_CBC_SHA256"},
      {TLSCipherSuite::RSA_WITH_CAMELLIA_256_CBC_SHA,
       "RSA_WITH_CAMELLIA_256_CBC_SHA"},
      {TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
       "DHE_RSA_WITH_CAMELLIA_256_CBC_SHA"},
      {TLSCipherSuite::PSK_WITH_AES_128_CBC_SHA, "PSK_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::PSK_WITH_AES_256_CBC_SHA, "PSK_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_128_CBC_SHA,
       "DHE_PSK_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_256_CBC_SHA,
       "DHE_PSK_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::RSA_PSK_WITH_AES_128_CBC_SHA,
       "RSA_PSK_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::RSA_PSK_WITH_AES_256_CBC_SHA,
       "RSA_PSK_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::RSA_WITH_AES_128_GCM_SHA256,
       "RSA_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::RSA_WITH_AES_256_GCM_SHA384,
       "RSA_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_128_GCM_SHA256,
       "DHE_RSA_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_256_GCM_SHA384,
       "DHE_RSA_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::PSK_WITH_AES_128_GCM_SHA256,
       "PSK_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::PSK_WITH_AES_256_GCM_SHA384,
       "PSK_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_128_GCM_SHA256,
       "DHE_PSK_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_256_GCM_SHA384,
       "DHE_PSK_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_AES_128_GCM_SHA256,
       "RSA_PSK_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_AES_256_GCM_SHA384,
       "RSA_PSK_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::PSK_WITH_AES_128_CBC_SHA256,
       "PSK_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::PSK_WITH_AES_256_CBC_SHA384,
       "PSK_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::PSK_WITH_NULL_SHA256, "PSK_WITH_NULL_SHA256"},
      {TLSCipherSuite::PSK_WITH_NULL_SHA384, "PSK_WITH_NULL_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_128_CBC_SHA256,
       "DHE_PSK_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_256_CBC_SHA384,
       "DHE_PSK_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_NULL_SHA256, "DHE_PSK_WITH_NULL_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_NULL_SHA384, "DHE_PSK_WITH_NULL_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_AES_128_CBC_SHA256,
       "RSA_PSK_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_AES_256_CBC_SHA384,
       "RSA_PSK_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_NULL_SHA256, "RSA_PSK_WITH_NULL_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_NULL_SHA384, "RSA_PSK_WITH_NULL_SHA384"},
      {TLSCipherSuite::RSA_WITH_CAMELLIA_128_CBC_SHA256,
       "RSA_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
       "DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::RSA_WITH_CAMELLIA_256_CBC_SHA256,
       "RSA_WITH_CAMELLIA_256_CBC_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
       "DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_NULL_SHA, "ECDH_ECDSA_WITH_NULL_SHA"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_AES_128_CBC_SHA,
       "ECDH_ECDSA_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_AES_256_CBC_SHA,
       "ECDH_ECDSA_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_NULL_SHA, "ECDHE_ECDSA_WITH_NULL_SHA"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
       "ECDHE_ECDSA_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
       "ECDHE_ECDSA_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::ECDH_RSA_WITH_NULL_SHA, "ECDH_RSA_WITH_NULL_SHA"},
      {TLSCipherSuite::ECDH_RSA_WITH_AES_128_CBC_SHA,
       "ECDH_RSA_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::ECDH_RSA_WITH_AES_256_CBC_SHA,
       "ECDH_RSA_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::ECDHE_RSA_WITH_NULL_SHA, "ECDHE_RSA_WITH_NULL_SHA"},
      {TLSCipherSuite::ECDHE_RSA_WITH_AES_128_CBC_SHA,
       "ECDHE_RSA_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::ECDHE_RSA_WITH_AES_256_CBC_SHA,
       "ECDHE_RSA_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
       "ECDHE_ECDSA_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
       "ECDHE_ECDSA_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
       "ECDH_ECDSA_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
       "ECDH_ECDSA_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_RSA_WITH_AES_128_CBC_SHA256,
       "ECDHE_RSA_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_RSA_WITH_AES_256_CBC_SHA384,
       "ECDHE_RSA_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::ECDH_RSA_WITH_AES_128_CBC_SHA256,
       "ECDH_RSA_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::ECDH_RSA_WITH_AES_256_CBC_SHA384,
       "ECDH_RSA_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
       "ECDHE_ECDSA_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
       "ECDHE_ECDSA_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
       "ECDH_ECDSA_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
       "ECDH_ECDSA_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_RSA_WITH_AES_128_GCM_SHA256,
       "ECDHE_RSA_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::ECDHE_RSA_WITH_AES_256_GCM_SHA384,
       "ECDHE_RSA_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::ECDH_RSA_WITH_AES_128_GCM_SHA256,
       "ECDH_RSA_WITH_AES_128_GCM_SHA256"},
      {TLSCipherSuite::ECDH_RSA_WITH_AES_256_GCM_SHA384,
       "ECDH_RSA_WITH_AES_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_PSK_WITH_AES_128_CBC_SHA,
       "ECDHE_PSK_WITH_AES_128_CBC_SHA"},
      {TLSCipherSuite::ECDHE_PSK_WITH_AES_256_CBC_SHA,
       "ECDHE_PSK_WITH_AES_256_CBC_SHA"},
      {TLSCipherSuite::ECDHE_PSK_WITH_AES_128_CBC_SHA256,
       "ECDHE_PSK_WITH_AES_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_PSK_WITH_AES_256_CBC_SHA384,
       "ECDHE_PSK_WITH_AES_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA, "ECDHE_PSK_WITH_NULL_SHA"},
      {TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA256,
       "ECDHE_PSK_WITH_NULL_SHA256"},
      {TLSCipherSuite::ECDHE_PSK_WITH_NULL_SHA384,
       "ECDHE_PSK_WITH_NULL_SHA384"},
      {TLSCipherSuite::RSA_WITH_ARIA_128_CBC_SHA256,
       "RSA_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::RSA_WITH_ARIA_256_CBC_SHA384,
       "RSA_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::DHE_RSA_WITH_ARIA_128_CBC_SHA256,
       "DHE_RSA_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_ARIA_256_CBC_SHA384,
       "DHE_RSA_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256,
       "ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384,
       "ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256,
       "ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384,
       "ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_RSA_WITH_ARIA_128_CBC_SHA256,
       "ECDHE_RSA_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_RSA_WITH_ARIA_256_CBC_SHA384,
       "ECDHE_RSA_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDH_RSA_WITH_ARIA_128_CBC_SHA256,
       "ECDH_RSA_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDH_RSA_WITH_ARIA_256_CBC_SHA384,
       "ECDH_RSA_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::RSA_WITH_ARIA_128_GCM_SHA256,
       "RSA_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::RSA_WITH_ARIA_256_GCM_SHA384,
       "RSA_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::DHE_RSA_WITH_ARIA_128_GCM_SHA256,
       "DHE_RSA_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_ARIA_256_GCM_SHA384,
       "DHE_RSA_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256,
       "ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384,
       "ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256,
       "ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384,
       "ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_RSA_WITH_ARIA_128_GCM_SHA256,
       "ECDHE_RSA_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDHE_RSA_WITH_ARIA_256_GCM_SHA384,
       "ECDHE_RSA_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDH_RSA_WITH_ARIA_128_GCM_SHA256,
       "ECDH_RSA_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDH_RSA_WITH_ARIA_256_GCM_SHA384,
       "ECDH_RSA_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::PSK_WITH_ARIA_128_CBC_SHA256,
       "PSK_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::PSK_WITH_ARIA_256_CBC_SHA384,
       "PSK_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_ARIA_128_CBC_SHA256,
       "DHE_PSK_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_ARIA_256_CBC_SHA384,
       "DHE_PSK_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_ARIA_128_CBC_SHA256,
       "RSA_PSK_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_ARIA_256_CBC_SHA384,
       "RSA_PSK_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::PSK_WITH_ARIA_128_GCM_SHA256,
       "PSK_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::PSK_WITH_ARIA_256_GCM_SHA384,
       "PSK_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_ARIA_128_GCM_SHA256,
       "DHE_PSK_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_ARIA_256_GCM_SHA384,
       "DHE_PSK_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_ARIA_128_GCM_SHA256,
       "RSA_PSK_WITH_ARIA_128_GCM_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_ARIA_256_GCM_SHA384,
       "RSA_PSK_WITH_ARIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_PSK_WITH_ARIA_128_CBC_SHA256,
       "ECDHE_PSK_WITH_ARIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_PSK_WITH_ARIA_256_CBC_SHA384,
       "ECDHE_PSK_WITH_ARIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
       "ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
       "ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
       "ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
       "ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
       "ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384,
       "ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256,
       "ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384,
       "ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::RSA_WITH_CAMELLIA_128_GCM_SHA256,
       "RSA_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::RSA_WITH_CAMELLIA_256_GCM_SHA384,
       "RSA_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256,
       "DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384,
       "DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256,
       "ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384,
       "ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256,
       "ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384,
       "ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256,
       "ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384,
       "ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256,
       "ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384,
       "ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::PSK_WITH_CAMELLIA_128_GCM_SHA256,
       "PSK_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::PSK_WITH_CAMELLIA_256_GCM_SHA384,
       "PSK_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256,
       "DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384,
       "DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256,
       "RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384,
       "RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384"},
      {TLSCipherSuite::PSK_WITH_CAMELLIA_128_CBC_SHA256,
       "PSK_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::PSK_WITH_CAMELLIA_256_CBC_SHA384,
       "PSK_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
       "DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
       "DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256,
       "RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384,
       "RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
       "ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256"},
      {TLSCipherSuite::ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
       "ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384"},
      {TLSCipherSuite::RSA_WITH_AES_128_CCM, "RSA_WITH_AES_128_CCM"},
      {TLSCipherSuite::RSA_WITH_AES_256_CCM, "RSA_WITH_AES_256_CCM"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_128_CCM, "DHE_RSA_WITH_AES_128_CCM"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_256_CCM, "DHE_RSA_WITH_AES_256_CCM"},
      {TLSCipherSuite::RSA_WITH_AES_128_CCM_8, "RSA_WITH_AES_128_CCM_8"},
      {TLSCipherSuite::RSA_WITH_AES_256_CCM_8, "RSA_WITH_AES_256_CCM_8"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_128_CCM_8,
       "DHE_RSA_WITH_AES_128_CCM_8"},
      {TLSCipherSuite::DHE_RSA_WITH_AES_256_CCM_8,
       "DHE_RSA_WITH_AES_256_CCM_8"},
      {TLSCipherSuite::PSK_WITH_AES_128_CCM, "PSK_WITH_AES_128_CCM"},
      {TLSCipherSuite::PSK_WITH_AES_256_CCM, "PSK_WITH_AES_256_CCM"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_128_CCM, "DHE_PSK_WITH_AES_128_CCM"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_256_CCM, "DHE_PSK_WITH_AES_256_CCM"},
      {TLSCipherSuite::PSK_WITH_AES_128_CCM_8, "PSK_WITH_AES_128_CCM_8"},
      {TLSCipherSuite::PSK_WITH_AES_256_CCM_8, "PSK_WITH_AES_256_CCM_8"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_128_CCM_8,
       "DHE_PSK_WITH_AES_128_CCM_8"},
      {TLSCipherSuite::DHE_PSK_WITH_AES_256_CCM_8,
       "DHE_PSK_WITH_AES_256_CCM_8"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CCM,
       "ECDHE_ECDSA_WITH_AES_128_CCM"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CCM,
       "ECDHE_ECDSA_WITH_AES_256_CCM"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_128_CCM_8,
       "ECDHE_ECDSA_WITH_AES_128_CCM_8"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_AES_256_CCM_8,
       "ECDHE_ECDSA_WITH_AES_256_CCM_8"},
      {TLSCipherSuite::ECJPAKE_WITH_AES_128_CCM_8,
       "ECJPAKE_WITH_AES_128_CCM_8"},
      {TLSCipherSuite::ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
       "ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
       "ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
       "DHE_RSA_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::PSK_WITH_CHACHA20_POLY1305_SHA256,
       "PSK_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256,
       "ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::DHE_PSK_WITH_CHACHA20_POLY1305_SHA256,
       "DHE_PSK_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::RSA_PSK_WITH_CHACHA20_POLY1305_SHA256,
       "RSA_PSK_WITH_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::TLS1_3_AES_128_GCM_SHA256, "TLS1_3_AES_128_GCM_SHA256"},
      {TLSCipherSuite::TLS1_3_AES_256_GCM_SHA384, "TLS1_3_AES_256_GCM_SHA384"},
      {TLSCipherSuite::TLS1_3_CHACHA20_POLY1305_SHA256,
       "TLS1_3_CHACHA20_POLY1305_SHA256"},
      {TLSCipherSuite::TLS1_3_AES_128_CCM_SHA256, "TLS1_3_AES_128_CCM_SHA256"},
      {TLSCipherSuite::TLS1_3_AES_128_CCM_8_SHA256,
       "TLS1_3_AES_128_CCM_8_SHA256"},
  };
  for (const auto &[value, expected] : table) {
    REQUIRE(TLSCipherSuite_toString(value) == expected);
  }
  // bogus -> UNKNOWN default branch
  REQUIRE(TLSCipherSuite_toString(static_cast<TLSCipherSuite>(0xFEED)) ==
          "UNKNOWN");
}

// ---------------------------------------------------------------------------
// src/types.cpp -- debug mode helpers
// ---------------------------------------------------------------------------
TEST_CASE("Debug mode global helpers", "[types]") {
  setDebug(Debug::None);
  REQUIRE(getDebug() == Debug::None);
  enableDebug(Debug::Server);
  REQUIRE(test(getDebug(), Debug::Server));
  enableDebug(Debug::Client);
  REQUIRE(test(getDebug(), Debug::Client));
  disableDebug(Debug::Server);
  REQUIRE_FALSE(test(getDebug(), Debug::Server));
  REQUIRE(test(getDebug(), Debug::Client));
  setDebug(Debug::None);

  // printDebugMessage: enabled branch prints
  enableDebug(Debug::Message);
  printDebugMessage(Debug::Message, "hello-test");
  // disabled branch: mode not set -> no print, returns silently
  printDebugMessage(Debug::Gil, "should-not-print");
  setDebug(Debug::None);
  printDebugMessage(Debug::Server, "none");
}

// ---------------------------------------------------------------------------
// src/types.cpp -- boolean, Byte32 and TimePoint string conversion
// ---------------------------------------------------------------------------
TEST_CASE("bool_toString", "[types]") {
  REQUIRE(bool_toString(true) == "True");
  REQUIRE(bool_toString(false) == "False");
}

TEST_CASE("Byte32_toString", "[types]") {
  Byte32 b(0x00000001u);
  auto s = Byte32_toString(b);
  REQUIRE(s.rfind("0b", 0) == 0);
  REQUIRE(s.size() == 2 + 32);
  Byte32 z(0);
  REQUIRE(Byte32_toString(z) == "0b00000000000000000000000000000000");
}

TEST_CASE("TimePoint_toString", "[types]") {
  auto tp = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(1700000000123));
  auto s = TimePoint_toString(tp);
  REQUIRE(s.size() > 0);
  // optional overload: engaged and empty
  std::optional<std::chrono::system_clock::time_point> opt = tp;
  auto so = TimePoint_toString(opt);
  REQUIRE_FALSE(so.empty());
  std::optional<std::chrono::system_clock::time_point> none = std::nullopt;
  REQUIRE(TimePoint_toString(none) == "None");
}

// ---------------------------------------------------------------------------
// src/types.cpp -- address/port assertions
// ---------------------------------------------------------------------------
TEST_CASE("Assert_IPv4", "[types]") {
  REQUIRE_NOTHROW(Assert_IPv4("127.0.0.1"));
  REQUIRE_NOTHROW(Assert_IPv4("255.255.255.255"));
  REQUIRE_NOTHROW(Assert_IPv4("0.0.0.0"));
  REQUIRE_NOTHROW(Assert_IPv4("localhost"));
  REQUIRE_NOTHROW(Assert_IPv4("lo"));
  REQUIRE_THROWS_AS(Assert_IPv4("not-an-ip"), std::invalid_argument);
  REQUIRE_THROWS_AS(Assert_IPv4("256.1.1.1"), std::invalid_argument);
  REQUIRE_THROWS_AS(Assert_IPv4("1.2.3"), std::invalid_argument);
}

TEST_CASE("Assert_Port", "[types]") {
  REQUIRE_NOTHROW(Assert_Port(1));
  REQUIRE_NOTHROW(Assert_Port(65534));
  REQUIRE_NOTHROW(Assert_Port(2404));
  REQUIRE_THROWS_AS(Assert_Port(0), std::invalid_argument);
  REQUIRE_THROWS_AS(Assert_Port(-1), std::invalid_argument);
  REQUIRE_THROWS_AS(Assert_Port(65535), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// src/types.cpp -- CP56Time2a <-> time_point conversion
// ---------------------------------------------------------------------------
TEST_CASE("to_time_point and from_time_point", "[types]") {
  sCP56Time2a storage{};
  CP56Time2a time = &storage;
  auto tp = std::chrono::system_clock::time_point(
      std::chrono::milliseconds(1700000000123));
  from_time_point(time, tp);
  auto back = to_time_point(time);
  // milliseconds round-trip
  auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
                    back.time_since_epoch())
                    .count();
  auto orig = std::chrono::duration_cast<std::chrono::milliseconds>(
                  tp.time_since_epoch())
                  .count();
  REQUIRE(millis == orig);
}

// ---------------------------------------------------------------------------
// src/types.cpp -- InfoValue / InfoQuality visitors (every alternative)
// ---------------------------------------------------------------------------
TEST_CASE("InfoValue_toString all alternatives", "[types]") {
  REQUIRE(InfoValue_toString(std::monostate{}) == "N.A.");
  REQUIRE(InfoValue_toString(false) == "0");
  REQUIRE(InfoValue_toString(true) == "1");
}

TEST_CASE("InfoValue_toString typed variants", "[types]") {
  REQUIRE(InfoValue_toString(DoublePointValue(IEC60870_DOUBLE_POINT_ON)) ==
          "ON");
  REQUIRE(InfoValue_toString(LimitedInt7(5)) == "5");
  REQUIRE(InfoValue_toString(StepCommandValue(IEC60870_STEP_LOWER)) == "LOWER");
  Byte32 b(42);
  REQUIRE(InfoValue_toString(b) == "42");
  REQUIRE(InfoValue_toString(NormalizedFloat(0.5f)) == std::to_string(0.5f));
  REQUIRE(InfoValue_toString(LimitedInt16(123)) == "123");
  REQUIRE(InfoValue_toString(EventState(IEC60870_EVENTSTATE_ON)) == "ON");
  REQUIRE(InfoValue_toString(StartEvents::General) ==
          "StartEvents set: { General }");
  REQUIRE(InfoValue_toString(OutputCircuits::General) ==
          "OutputCircuit set: { General }");
  REQUIRE(InfoValue_toString(FieldSet16::I0) == "PackedSingle set: { I0 }");
  REQUIRE(InfoValue_toString(1.5f) == std::to_string(1.5f));
  REQUIRE(InfoValue_toString(static_cast<int32_t>(-7)) == std::to_string(-7));
}

TEST_CASE("InfoQuality_toString all alternatives", "[types]") {
  REQUIRE(InfoQuality_toString(std::monostate{}) == "N. A.");
  REQUIRE(InfoQuality_toString(Quality::None) ==
          "Quality set: {}, is_good: True");
  REQUIRE(InfoQuality_toString(Quality::Invalid) ==
          "Quality set: { Invalid }, is_good: False");
  REQUIRE(InfoQuality_toString(BinaryCounterQuality::Carry) ==
          "BinaryCounterQuality set: { Carry }, is_good: False");
}
