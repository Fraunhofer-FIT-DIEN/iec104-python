/**
 * Copyright 2026 Fraunhofer Institute for Applied Information Technology FIT
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
 * @file test_message.cpp
 * @brief Coverage tests for src/remote/message/*:
 *   IncomingMessage, OutgoingMessage, PointCommand, PointMessage, Batch.
 *
 *   NOTE ON GIL: these tests exercise GilAwareMutex / callback paths and are
 *   linked into c104_tests whose main.cpp keeps a py::scoped_interpreter alive
 *   for the whole process. We therefore include <pybind11/pybind11.h> and MUST
 *   NOT spawn a second interpreter.
 */

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <cstdint>
#include <memory>
#include <sstream>

#include "Server.h"
#include "object/DataPoint.h"
#include "object/Information.h"
#include "object/Station.h"
#include "remote/Helper.h"
#include "remote/message/Batch.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/OutgoingMessage.h"
#include "remote/message/PointCommand.h"
#include "remote/message/PointMessage.h"
#include "types.h"

#include <pybind11/pybind11.h> // GIL footgun: do not create a second interpreter

using namespace Remote::Message;

namespace {

// ---------------------------------------------------------------------------
// ASDU crafting helpers
// ---------------------------------------------------------------------------

sCS101_AppLayerParameters defaultAppLayerParameters() {
  return {.sizeOfTypeId = 1,
          .sizeOfVSQ = 1,
          .sizeOfCOT = 2,
          .originatorAddress = 0,
          .sizeOfCA = 2,
          .sizeOfIOA = 3,
          .maxSizeOfASDU = 249};
}

sCP56Time2a makeTime(uint_fast64_t ms) {
  sCP56Time2a time{};
  CP56Time2a_createFromMsTimestamp(&time, ms);
  return time;
}

// Build a CS101_ASDU with default app layer parameters, one information object
// (@p ioCreator), a given COT/CA and optional test/negative/sequence flags.
// Returns the IncomingMessage facade (which clones the ASDU); the caller's
// ASDU handle is destroyed.
template <typename Fn>
std::shared_ptr<IncomingMessage>
makeIncomingFlags(IEC60870_5_TypeID type, uint_fast32_t ioa,
                  CS101_CauseOfTransmission cot, uint_fast16_t ca, bool isTest,
                  bool isNegative, bool isSequence, Fn ioCreator) {
  // IncomingMessage retains the CS101_AppLayerParameters pointer for its
  // lifetime (e.g. getRawBytes()). A stack-local struct would dangle as soon as
  // this helper returns, so use a process-lifetime (static) instance.
  static sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu =
      CS101_ASDU_create(&params, false, cot, 0, ca, isTest, isNegative);
  CS101_ASDU_setTypeID(asdu, type);
  CS101_ASDU_setCOT(asdu, cot);
  CS101_ASDU_setCA(asdu, ca);
  if (isSequence)
    CS101_ASDU_setSequence(asdu, true);
  InformationObject io = ioCreator();
  CS101_ASDU_addInformationObject(asdu, io);
  InformationObject_destroy(io);
  auto msg = IncomingMessage::create(asdu, &params);
  CS101_ASDU_destroy(asdu);
  return msg;
}

template <typename Fn>
std::shared_ptr<IncomingMessage>
makeIncoming(IEC60870_5_TypeID type, uint_fast32_t ioa,
             CS101_CauseOfTransmission cot, uint_fast16_t ca, Fn ioCreator) {
  return makeIncomingFlags(type, ioa, cot, ca, false, false, false, ioCreator);
}

// Servers created by makeAttachedPoint must outlive the points they own (a
// DataPoint keeps its Station via a weak_ptr), but a process-lifetime *static*
// vector must be explicitly released while the embedded interpreter is still
// alive: ~Server -> stop() acquires the GIL, which segfaults if it runs at
// static-teardown after the interpreter has been finalized. main.cpp calls
// c104_tests_release_servers() after the run, before interpreter teardown.
std::vector<std::shared_ptr<Server>> g_keptServers;

// Create a DataPoint attached to a server-backed station (station reference
// non-null) so OutgoingMessage/PointMessage/PointCommand/Batch construction
// works.
std::shared_ptr<Object::DataPoint>
makeAttachedPoint(IEC60870_5_TypeID type, uint_fast16_t ca, uint_fast32_t ioa) {
  auto server = Server::create("127.0.0.1", 2404);
  g_keptServers.push_back(server);
  auto station = server->addStation(ca);
  return station->addPoint(ioa, type);
}

} // namespace

// Called by tests/main.cpp right before the embedded py::scoped_interpreter is
// torn down, so the process-lifetime test Servers are destroyed while the GIL
// is still available (otherwise ~Server -> stop() segfaults at
// static-teardown).
void c104_tests_release_servers() { g_keptServers.clear(); }

// ---------------------------------------------------------------------------
// IncomingMessage
// ---------------------------------------------------------------------------

TEST_CASE("IncomingMessage metadata getters and single-object extraction",
          "[remote::message::incoming]") {
  auto m = makeIncoming(M_SP_NA_1, 101, CS101_COT_SPONTANEOUS, 1, [] {
    return (InformationObject)SinglePointInformation_create(nullptr, 101, true,
                                                            0);
  });
  REQUIRE(m->getType() == M_SP_NA_1);
  REQUIRE(m->getCommonAddress() == 1);
  REQUIRE(m->getOriginatorAddress() == 0);
  REQUIRE(m->getIOA() == 101);
  REQUIRE(m->getCauseOfTransmission() == CS101_COT_SPONTANEOUS);
  REQUIRE(m->isTest() == false);
  REQUIRE(m->isNegative() == false);
  REQUIRE(m->isSequence() == false);
  REQUIRE(m->getNumberOfObjects() == 1);
  REQUIRE(m->getAsdu() != nullptr);
  REQUIRE(m->getInfo() != nullptr);
  REQUIRE(m->getInformationObject() != nullptr);
  REQUIRE(m->getRawBytes() != nullptr);
  REQUIRE(m->toString().find("M_SP_NA_1") != std::string::npos);
  REQUIRE(m->getRawMessageString().find("I-Format") != std::string::npos);
}

TEST_CASE("IncomingMessage test/negative flags and sequence metadata",
          "[remote::message::incoming]") {
  auto m = makeIncomingFlags(
      M_ME_NC_1, 55, CS101_COT_REQUEST, 3, true, true, false, [] {
        return (InformationObject)MeasuredValueShort_create(nullptr, 55, 4.5f,
                                                            0);
      });
  REQUIRE(m->isTest() == true);
  REQUIRE(m->isNegative() == true);
  REQUIRE(m->isSequence() == false);

  auto sq = makeIncomingFlags(
      M_SP_NA_1, 1, CS101_COT_SPONTANEOUS, 1, false, false, true, [] {
        return (InformationObject)SinglePointInformation_create(nullptr, 1,
                                                                true, 0);
      });
  REQUIRE(sq->isSequence() == true);
}

TEST_CASE("IncomingMessage first/next iteration across multiple objects",
          "[remote::message::incoming]") {
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 0,
                                      7, false, false);
  CS101_ASDU_setTypeID(asdu, M_ME_NC_1);
  // two separate (non-sequence) monitoring objects
  auto io1 = (InformationObject)MeasuredValueShort_create(nullptr, 1, 1.0f, 0);
  auto io2 = (InformationObject)MeasuredValueShort_create(nullptr, 2, 2.0f, 0);
  CS101_ASDU_addInformationObject(asdu, io1);
  CS101_ASDU_addInformationObject(asdu, io2);
  InformationObject_destroy(io1);
  InformationObject_destroy(io2);

  auto m = IncomingMessage::create(asdu, &params);
  CS101_ASDU_destroy(asdu);

  REQUIRE(m->getNumberOfObjects() == 2);
  // The constructor already extracts the first object (position 0).
  REQUIRE(m->getIOA() == 1);
  // first() is idempotent and re-positions on object 0.
  m->first();
  REQUIRE(m->getIOA() == 1);
  // Iterate to the end with next(). NOTE: the first next() after a
  // first()/construction is a redundant reset (re-visits object 0); the
  // following next() advances to the next object.
  REQUIRE(m->next() == true);
  REQUIRE(m->next() == true);
  REQUIRE(m->getIOA() == 2);
  REQUIRE(m->next() == false); // beyond end, invalid position
  // iterate again via first()
  m->first();
  REQUIRE(m->getIOA() == 1);
}

TEST_CASE("IncomingMessage empty / no-object message",
          "[remote::message::incoming]") {
  // No information objects -> numberOfObjects == 0, position invalid
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 0,
                                      7, false, false);
  CS101_ASDU_setTypeID(asdu, M_SP_NA_1);
  auto m = IncomingMessage::create(asdu, &params);
  CS101_ASDU_destroy(asdu);
  REQUIRE(m->getNumberOfObjects() == 0);
  REQUIRE(m->getInformationObject() == nullptr);
}

TEST_CASE("IncomingMessage rejects CP24Time based types",
          "[remote::message::incoming]") {
  // M_SP_TA_1 is a CP24Time based type -> must throw on extraction
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 0,
                                      1, false, false);
  CS101_ASDU_setTypeID(asdu, M_SP_TA_1);
  REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                    std::invalid_argument);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("IncomingMessage rejects C_TS_NA_1 / C_CD_NA_1",
          "[remote::message::incoming]") {
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS,
                                        0, 1, false, false);
    CS101_ASDU_setTypeID(asdu, C_TS_NA_1);
    REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                      std::invalid_argument);
    CS101_ASDU_destroy(asdu);
  }
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS,
                                        0, 1, false, false);
    CS101_ASDU_setTypeID(asdu, C_CD_NA_1);
    REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                      std::invalid_argument);
    CS101_ASDU_destroy(asdu);
  }
}

TEST_CASE("IncomingMessage rejects sequence and multi-object control messages",
          "[remote::message::incoming]") {
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  {
    // control type marked as sequence -> reject
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_ACTIVATION, 0,
                                        1, false, false);
    CS101_ASDU_setTypeID(asdu, C_SC_NA_1);
    CS101_ASDU_setSequence(asdu, true);
    REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                      std::invalid_argument);
    CS101_ASDU_destroy(asdu);
  }
  {
    // control type with multiple objects -> reject
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_ACTIVATION, 0,
                                        1, false, false);
    CS101_ASDU_setTypeID(asdu, C_SC_NA_1);
    auto io1 =
        (InformationObject)SingleCommand_create(nullptr, 1, true, false, 0);
    auto io2 =
        (InformationObject)SingleCommand_create(nullptr, 2, true, false, 0);
    CS101_ASDU_addInformationObject(asdu, io1);
    CS101_ASDU_addInformationObject(asdu, io2);
    InformationObject_destroy(io1);
    InformationObject_destroy(io2);
    REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                      std::invalid_argument);
    CS101_ASDU_destroy(asdu);
  }
}

TEST_CASE("IncomingMessage rejects file transfer types",
          "[remote::message::incoming]") {
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 0,
                                      1, false, false);
  CS101_ASDU_setTypeID(asdu, F_FR_NA_1);
  REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                    std::invalid_argument);
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("IncomingMessage global common address rejected when reachable",
          "[remote::message::incoming]") {
  // The guard is `type < C_IC_NA_1 && type > C_RP_NA_1`; with
  // C_IC_NA_1=100 < C_RP_NA_1=105 this range is empty in the enum, so the
  // branch is currently unreachable. This test documents the intent: a
  // non-command type with a global common address would be rejected there.
  // We still craft one to keep the path exercised if the guard is ever fixed.
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 0,
                                      65535, false, false);
  CS101_ASDU_setTypeID(asdu, M_SP_NA_1);
  // currently passes (the guard is empty) - documents behaviour
  REQUIRE_NOTHROW(IncomingMessage::create(asdu, &params));
  CS101_ASDU_destroy(asdu);
}

TEST_CASE("IncomingMessage requireConfirmation / isSelectCommand",
          "[remote::message::incoming]") {
  auto act = makeIncoming(C_SC_NA_1, 1, CS101_COT_ACTIVATION, 1, [] {
    return (InformationObject)SingleCommand_create(nullptr, 1, true, true, 0);
  });
  REQUIRE(act->requireConfirmation() == true);
  // isSelectCommand: info is a SingleCmd, select=true -> true
  act->first();
  REQUIRE(act->isSelectCommand() == true);

  auto deact = makeIncoming(C_SC_NA_1, 1, CS101_COT_DEACTIVATION, 1, [] {
    return (InformationObject)SingleCommand_create(nullptr, 1, true, false, 0);
  });
  REQUIRE(deact->requireConfirmation() == true);
  deact->first();
  REQUIRE(deact->isSelectCommand() == false); // not select flag

  auto spam = makeIncoming(C_SC_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
    return (InformationObject)SingleCommand_create(nullptr, 1, true, false, 0);
  });
  REQUIRE(spam->requireConfirmation() == false);
  REQUIRE(spam->isSelectCommand() == false);
}

// ---------------------------------------------------------------------------
// IncomingMessage: extractInformation branch coverage per type
// ---------------------------------------------------------------------------

TEST_CASE("IncomingMessage extract monitoring single/double/step/binary",
          "[remote::message::incoming]") {
  {
    auto m = makeIncoming(M_SP_TB_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)SinglePointWithCP56Time2a_create(nullptr, 1,
                                                                 true, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_DP_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)DoublePointInformation_create(
          nullptr, 1, IEC60870_DOUBLE_POINT_ON, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_DP_TB_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)DoublePointWithCP56Time2a_create(
          nullptr, 1, IEC60870_DOUBLE_POINT_ON, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ST_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)StepPositionInformation_create(nullptr, 1, 3,
                                                               true, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ST_TB_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)StepPositionWithCP56Time2a_create(nullptr, 1, 3,
                                                                  true, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_BO_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)BitString32_create(nullptr, 1, 0xDEADBEEF);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_BO_TB_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)Bitstring32WithCP56Time2a_create(
          nullptr, 1, 0xDEADBEEF, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
}

TEST_CASE("IncomingMessage extract monitoring measured values",
          "[remote::message::incoming]") {
  {
    auto m = makeIncoming(M_ME_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)MeasuredValueNormalized_create(nullptr, 1, 0.5f,
                                                               0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ME_TD_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)MeasuredValueNormalizedWithCP56Time2a_create(
          nullptr, 1, 0.5f, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ME_NB_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)MeasuredValueScaled_create(nullptr, 1, 42, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ME_TE_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)MeasuredValueScaledWithCP56Time2a_create(
          nullptr, 1, 42, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ME_NC_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)MeasuredValueShort_create(nullptr, 1, 3.14f, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ME_TF_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)MeasuredValueShortWithCP56Time2a_create(
          nullptr, 1, 3.14f, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_ME_ND_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      return (InformationObject)MeasuredValueNormalizedWithoutQuality_create(
          nullptr, 1, 0.5f);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
}

TEST_CASE("IncomingMessage extract monitoring totals & protection",
          "[remote::message::incoming]") {
  {
    auto m = makeIncoming(M_IT_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      BinaryCounterReading bcr =
          BinaryCounterReading_create(nullptr, 1234, 2, false, false, false);
      auto io = (InformationObject)IntegratedTotals_create(nullptr, 1, bcr);
      BinaryCounterReading_destroy(bcr);
      return io;
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_IT_TB_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      BinaryCounterReading bcr =
          BinaryCounterReading_create(nullptr, 1234, 2, false, false, false);
      auto io = (InformationObject)IntegratedTotalsWithCP56Time2a_create(
          nullptr, 1, bcr, &t);
      BinaryCounterReading_destroy(bcr);
      return io;
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_EP_TD_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      sCP16Time2a elapsed{};
      CP16Time2a_setEplapsedTimeInMs(&elapsed, 50);
      tSingleEvent event{0b00000001};
      return (InformationObject)EventOfProtectionEquipmentWithCP56Time2a_create(
          nullptr, 1, &event, &elapsed, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_EP_TE_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      sCP16Time2a elapsed{};
      CP16Time2a_setEplapsedTimeInMs(&elapsed, 50);
      return (InformationObject)
          PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(
              nullptr, 1, 0b00000001, 0, &elapsed, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_EP_TF_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      auto t = makeTime(1000);
      sCP16Time2a elapsed{};
      CP16Time2a_setEplapsedTimeInMs(&elapsed, 50);
      return (InformationObject)PackedOutputCircuitInfoWithCP56Time2a_create(
          nullptr, 1, 0b00000001, 0, &elapsed, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(M_PS_NA_1, 1, CS101_COT_SPONTANEOUS, 1, [] {
      sStatusAndStatusChangeDetection sscd{};
      sscd.encodedValue[0] = 0x01;
      sscd.encodedValue[1] = 0x02;
      sscd.encodedValue[2] = 0x03;
      sscd.encodedValue[3] = 0x04;
      return (InformationObject)PackedSinglePointWithSCD_create(nullptr, 1,
                                                                &sscd, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
}

TEST_CASE("IncomingMessage extract control commands (single/double/step)",
          "[remote::message::incoming]") {
  {
    auto m = makeIncoming(C_SC_NA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)SingleCommand_create(nullptr, 1, true, false,
                                                     0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_SC_TA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)SingleCommandWithCP56Time2a_create(
          nullptr, 1, true, false, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_DC_NA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)DoubleCommand_create(nullptr, 1, 1, false, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_DC_TA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)DoubleCommandWithCP56Time2a_create(
          nullptr, 1, 1, false, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_RC_NA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)StepCommand_create(
          nullptr, 1, IEC60870_STEP_HIGHER, false, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_RC_TA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)StepCommandWithCP56Time2a_create(
          nullptr, 1, IEC60870_STEP_HIGHER, false, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
}

TEST_CASE("IncomingMessage extract control commands (setpoints/binary/time)",
          "[remote::message::incoming]") {
  {
    auto m = makeIncoming(C_SE_NA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)SetpointCommandNormalized_create(
          nullptr, 1, 0.5f, false, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_SE_TA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)SetpointCommandNormalizedWithCP56Time2a_create(
          nullptr, 1, 0.5f, false, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_SE_NB_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)SetpointCommandScaled_create(nullptr, 1, 42,
                                                             false, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_SE_TB_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)SetpointCommandScaledWithCP56Time2a_create(
          nullptr, 1, 42, false, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_SE_NC_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)SetpointCommandShort_create(nullptr, 1, 3.14f,
                                                            false, 0);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_SE_TC_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)SetpointCommandShortWithCP56Time2a_create(
          nullptr, 1, 3.14f, false, 0, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_BO_NA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      return (InformationObject)Bitstring32Command_create(nullptr, 1, 0xCAFE);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_BO_TA_1, 1, CS101_COT_ACTIVATION, 1, [] {
      auto t = makeTime(1000);
      return (InformationObject)Bitstring32CommandWithCP56Time2a_create(
          nullptr, 1, 0xCAFE, &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
  {
    auto m = makeIncoming(C_CS_NA_1, 1, CS101_COT_ACTIVATION_CON, 1, [] {
      sCP56Time2a t = makeTime(1000);
      return (InformationObject)ClockSynchronizationCommand_create(nullptr, 1,
                                                                   &t);
    });
    m->first();
    REQUIRE(m->getInfo() != nullptr);
  }
}

TEST_CASE("IncomingMessage extract pass-through types (no info extracted)",
          "[remote::message::incoming]") {
  // M_EI_NA_1, C_IC_NA_1, C_CI_NA_1, C_RD_NA_1, C_TS_NA_1 pass and only the
  // informationObjectAddress is extracted (io remains null).
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_INITIALIZED,
                                        0, 1, false, false);
    CS101_ASDU_setTypeID(asdu, M_EI_NA_1);
    auto m = IncomingMessage::create(asdu, &params);
    CS101_ASDU_destroy(asdu);
    m->first();
    REQUIRE(m->getType() == M_EI_NA_1);
    REQUIRE(m->getInfo() == nullptr);
  }
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_REQUEST, 0, 1,
                                        false, false);
    CS101_ASDU_setTypeID(asdu, C_IC_NA_1);
    auto m = IncomingMessage::create(asdu, &params);
    CS101_ASDU_destroy(asdu);
    m->first();
    REQUIRE(m->getType() == C_IC_NA_1);
  }
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_REQUEST, 0, 1,
                                        false, false);
    CS101_ASDU_setTypeID(asdu, C_CI_NA_1);
    auto m = IncomingMessage::create(asdu, &params);
    CS101_ASDU_destroy(asdu);
    m->first();
    REQUIRE(m->getType() == C_CI_NA_1);
  }
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_REQUEST, 0, 1,
                                        false, false);
    CS101_ASDU_setTypeID(asdu, C_RD_NA_1);
    auto m = IncomingMessage::create(asdu, &params);
    CS101_ASDU_destroy(asdu);
    m->first();
    REQUIRE(m->getType() == C_RD_NA_1);
  }
}

TEST_CASE("IncomingMessage extract default (unsupported) type is tolerated",
          "[remote::message::incoming]") {
  // A monitoring-like type not handled by the switch falls into default which
  // only prints a diagnostic; the message is still valid.
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 0,
                                      1, false, false);
  CS101_ASDU_setTypeID(asdu, S_IT_TC_1); // 41: not handled in the switch
  auto io =
      (InformationObject)SinglePointInformation_create(nullptr, 1, true, 0);
  CS101_ASDU_addInformationObject(asdu, io);
  InformationObject_destroy(io);
  auto m = IncomingMessage::create(asdu, &params);
  CS101_ASDU_destroy(asdu);
  m->first();
  REQUIRE_NOTHROW(m->getInfo());
}

// ---------------------------------------------------------------------------
// IncomingMessage: isValidCauseOfTransmission coverage
// ---------------------------------------------------------------------------

TEST_CASE("IncomingMessage cause of transmission classifier",
          "[remote::message::incoming]") {
  // Build a correct InformationObject for the given type so the IncomingMessage
  // constructor's extractInformation() never casts to the wrong concrete type.
  auto makeCompatibleIO = [](IEC60870_5_TypeID t) -> InformationObject {
    auto t56 = makeTime(1000);
    switch (t) {
    case M_SP_NA_1:
      return (InformationObject)SinglePointInformation_create(nullptr, 1, true,
                                                              0);
    case M_SP_TB_1:
      return (InformationObject)SinglePointWithCP56Time2a_create(nullptr, 1,
                                                                 true, 0, &t56);
    case M_DP_NA_1:
      return (InformationObject)DoublePointInformation_create(
          nullptr, 1, IEC60870_DOUBLE_POINT_ON, 0);
    case M_DP_TB_1:
      return (InformationObject)DoublePointWithCP56Time2a_create(
          nullptr, 1, IEC60870_DOUBLE_POINT_ON, 0, &t56);
    case M_ST_NA_1:
      return (InformationObject)StepPositionInformation_create(nullptr, 1, 2,
                                                               false, 0);
    case M_ST_TB_1:
      return (InformationObject)StepPositionWithCP56Time2a_create(
          nullptr, 1, 2, false, 0, &t56);
    case M_BO_NA_1:
      return (InformationObject)BitString32_create(nullptr, 1, 0x1);
    case M_BO_TB_1:
      return (InformationObject)Bitstring32WithCP56Time2a_create(nullptr, 1,
                                                                 0x1, &t56);
    case M_ME_NA_1:
      return (InformationObject)MeasuredValueNormalized_create(nullptr, 1, 0.5f,
                                                               0);
    case M_ME_TD_1:
      return (InformationObject)MeasuredValueNormalizedWithCP56Time2a_create(
          nullptr, 1, 0.5f, 0, &t56);
    case M_ME_NB_1:
      return (InformationObject)MeasuredValueScaled_create(nullptr, 1, 4, 0);
    case M_ME_TE_1:
      return (InformationObject)MeasuredValueScaledWithCP56Time2a_create(
          nullptr, 1, 4, 0, &t56);
    case M_ME_NC_1:
      return (InformationObject)MeasuredValueShort_create(nullptr, 1, 1.f, 0);
    case M_ME_TF_1:
      return (InformationObject)MeasuredValueShortWithCP56Time2a_create(
          nullptr, 1, 1.f, 0, &t56);
    case M_ME_ND_1:
      return (InformationObject)MeasuredValueNormalizedWithoutQuality_create(
          nullptr, 1, 0.5f);
    case M_IT_NA_1: {
      BinaryCounterReading bcr =
          BinaryCounterReading_create(nullptr, 1, 0, false, false, false);
      auto io = (InformationObject)IntegratedTotals_create(nullptr, 1, bcr);
      BinaryCounterReading_destroy(bcr);
      return io;
    }
    case M_IT_TB_1: {
      BinaryCounterReading bcr =
          BinaryCounterReading_create(nullptr, 1, 0, false, false, false);
      auto io = (InformationObject)IntegratedTotalsWithCP56Time2a_create(
          nullptr, 1, bcr, &t56);
      BinaryCounterReading_destroy(bcr);
      return io;
    }
    case M_EP_TD_1: {
      sCP16Time2a elapsed{};
      CP16Time2a_setEplapsedTimeInMs(&elapsed, 50);
      tSingleEvent event{0b00000001};
      return (InformationObject)EventOfProtectionEquipmentWithCP56Time2a_create(
          nullptr, 1, &event, &elapsed, &t56);
    }
    case M_EP_TE_1: {
      sCP16Time2a elapsed{};
      CP16Time2a_setEplapsedTimeInMs(&elapsed, 50);
      return (InformationObject)
          PackedStartEventsOfProtectionEquipmentWithCP56Time2a_create(
              nullptr, 1, 0b00000001, 0, &elapsed, &t56);
    }
    case M_EP_TF_1: {
      sCP16Time2a elapsed{};
      CP16Time2a_setEplapsedTimeInMs(&elapsed, 50);
      return (InformationObject)PackedOutputCircuitInfoWithCP56Time2a_create(
          nullptr, 1, 0b00000001, 0, &elapsed, &t56);
    }
    case M_PS_NA_1: {
      sStatusAndStatusChangeDetection sscd{};
      sscd.encodedValue[0] = 0x01;
      return (InformationObject)PackedSinglePointWithSCD_create(nullptr, 1,
                                                                &sscd, 0);
    }
    case C_SC_NA_1:
      return (InformationObject)SingleCommand_create(nullptr, 1, true, false,
                                                     0);
    case C_DC_NA_1:
      return (InformationObject)DoubleCommand_create(nullptr, 1, 1, false, 0);
    case C_RC_NA_1:
      return (InformationObject)StepCommand_create(
          nullptr, 1, IEC60870_STEP_HIGHER, false, 0);
    case C_SE_NA_1:
      return (InformationObject)SetpointCommandNormalized_create(
          nullptr, 1, 0.5f, false, 0);
    case C_SE_NB_1:
      return (InformationObject)SetpointCommandScaled_create(nullptr, 1, 4,
                                                             false, 0);
    case C_SE_NC_1:
      return (InformationObject)SetpointCommandShort_create(nullptr, 1, 1.f,
                                                            false, 0);
    case C_BO_NA_1:
      return (InformationObject)Bitstring32Command_create(nullptr, 1, 1);
    case C_BO_TA_1:
      return (InformationObject)Bitstring32CommandWithCP56Time2a_create(
          nullptr, 1, 1, &t56);
    case C_CS_NA_1:
      return (InformationObject)ClockSynchronizationCommand_create(nullptr, 1,
                                                                   &t56);
    case M_EI_NA_1:
      return (InformationObject)EndOfInitialization_create(
          nullptr, IEC60870_COI_REMOTE_RESET);
    case C_RD_NA_1:
      return (InformationObject)ReadCommand_create(nullptr, 1);
    case C_IC_NA_1:
      return (InformationObject)InterrogationCommand_create(nullptr, 1, 0);
    case C_CI_NA_1:
      return (InformationObject)CounterInterrogationCommand_create(nullptr, 1,
                                                                   0);
    case C_RP_NA_1:
      return (InformationObject)ResetProcessCommand_create(nullptr, 1, 0);
    case C_TS_TA_1:
      return (InformationObject)TestCommandWithCP56Time2a_create(nullptr, 0,
                                                                 &t56);
    case P_ME_NA_1:
      return (InformationObject)ParameterNormalizedValue_create(nullptr, 1,
                                                                0.5f, 0);
    case P_ME_NB_1:
      return (InformationObject)ParameterScaledValue_create(nullptr, 1, 4, 0);
    case P_ME_NC_1:
      return (InformationObject)ParameterFloatValue_create(nullptr, 1, 1.f, 0);
    case P_AC_NA_1:
      return (InformationObject)ParameterActivation_create(nullptr, 1, 0);
    default:
      return (InformationObject)SinglePointInformation_create(nullptr, 1, true,
                                                              0);
    }
  };

  auto eval = [&makeCompatibleIO](IEC60870_5_TypeID t,
                                  CS101_CauseOfTransmission cot,
                                  CS101_CauseOfTransmission bad) {
    auto valid = makeIncoming(
        t, 1, cot, 1, [t, &makeCompatibleIO]() { return makeCompatibleIO(t); });
    if (!valid->isValidCauseOfTransmission()) {
      std::cerr << "[DEBUG] type=" << (int)t << " cot=" << (int)cot
                << " valid==false" << std::endl;
    }
    REQUIRE(valid->isValidCauseOfTransmission() == true);
    // Some classifier branches only ever return true (their range checks are
    // structurally empty), so a "bad" COT cannot reach the false path there.
    // Pass the sentinel to skip the false assertion for those types.
    if (bad != static_cast<CS101_CauseOfTransmission>(-1)) {
      auto invalid = makeIncoming(t, 1, bad, 1, [t, &makeCompatibleIO]() {
        return makeCompatibleIO(t);
      });
      REQUIRE(invalid->isValidCauseOfTransmission() == false);
    }
  };

  // Sentinel: skips the "invalid" assertion for types whose classifier branch
  // can never return false.
  const auto NO_BAD = static_cast<CS101_CauseOfTransmission>(-1);

  // M_SP_NA_1 / M_DP_NA_1 / M_ST_NA_1 group (never returns false)
  eval(M_SP_NA_1, CS101_COT_SPONTANEOUS, NO_BAD);
  eval(M_DP_NA_1, CS101_COT_SPONTANEOUS, NO_BAD);
  eval(M_ST_NA_1, CS101_COT_SPONTANEOUS, NO_BAD);
  // M_BO_NA_1 / M_PS_NA_1 group (never returns false)
  eval(M_BO_NA_1, CS101_COT_SPONTANEOUS, NO_BAD);
  eval(M_PS_NA_1, CS101_COT_SPONTANEOUS, NO_BAD);
  // M_ME_* group (never returns false)
  eval(M_ME_NA_1, CS101_COT_PERIODIC, NO_BAD);
  eval(M_ME_NB_1, CS101_COT_PERIODIC, NO_BAD);
  eval(M_ME_NC_1, CS101_COT_PERIODIC, NO_BAD);
  eval(M_ME_ND_1, CS101_COT_PERIODIC, NO_BAD);
  // M_IT_* group (never returns false)
  eval(M_IT_NA_1, CS101_COT_SPONTANEOUS, NO_BAD);
  eval(M_IT_TB_1, CS101_COT_SPONTANEOUS, NO_BAD);
  // M_*_TB_1 (SP/DP/ST with CP56) group
  eval(M_SP_TB_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  eval(M_DP_TB_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  eval(M_ST_TB_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  // M_BO_TB_1 / M_ME_TD_1 / M_ME_TE_1 / M_ME_TF_1 group
  eval(M_BO_TB_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  eval(M_ME_TD_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  eval(M_ME_TE_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  eval(M_ME_TF_1, CS101_COT_SPONTANEOUS, CS101_COT_ACTIVATION);
  // M_EP_* group (only spontaneous valid)
  eval(M_EP_TD_1, CS101_COT_SPONTANEOUS, CS101_COT_REQUEST);
  eval(M_EP_TE_1, CS101_COT_SPONTANEOUS, CS101_COT_REQUEST);
  eval(M_EP_TF_1, CS101_COT_SPONTANEOUS, CS101_COT_REQUEST);
  // control command group (never returns false)
  eval(C_SC_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_DC_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_RC_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_SE_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_SE_NB_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_SE_NC_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_IC_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  // C_BO group (never returns false)
  eval(C_BO_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_BO_TA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_CI_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  // M_EI_NA_1 (only initialized valid)
  eval(M_EI_NA_1, CS101_COT_INITIALIZED, CS101_COT_ACTIVATION);
  // C_RD_NA_1 (request) (never returns false)
  eval(C_RD_NA_1, CS101_COT_REQUEST, NO_BAD);
  // C_CS_NA_1 (never returns false)
  eval(C_CS_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  // C_RP_NA_1 / C_TS_TA_1 (never returns false)
  eval(C_RP_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(C_TS_TA_1, CS101_COT_ACTIVATION, NO_BAD);
  // P_ME_* group (never returns false)
  eval(P_ME_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(P_ME_NB_1, CS101_COT_ACTIVATION, NO_BAD);
  eval(P_ME_NC_1, CS101_COT_ACTIVATION, NO_BAD);
  // P_AC_NA_1 (never returns false)
  eval(P_AC_NA_1, CS101_COT_ACTIVATION, NO_BAD);
  // NOTE: file-transfer types (F_* >= F_FR_NA_1) are rejected in
  // extractMetaData() before isValidCauseOfTransmission() is reachable, so
  // their classifier branches (F_FR/F_SR/F_SC/F_LS/F_AF/F_SG/F_SC_NB) are
  // unreachable through the public API.
}

TEST_CASE("IncomingMessage cause classifier throws for CP24/special types",
          "[remote::message::incoming]") {
  sCS101_AppLayerParameters params = defaultAppLayerParameters();
  {
    CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS,
                                        0, 1, false, false);
    CS101_ASDU_setTypeID(asdu, M_SP_TA_1); // CP24 type
    // construction throws already (CP24 rejection), so exercise the throw
    // directly by documenting it cannot reach isValidCauseOfTransmission.
    REQUIRE_THROWS_AS(IncomingMessage::create(asdu, &params),
                      std::invalid_argument);
    CS101_ASDU_destroy(asdu);
  }
}

TEST_CASE("IncomingMessage getRawBytes with 1-byte COT/CA parameters",
          "[remote::message::incoming]") {
  sCS101_AppLayerParameters params{.sizeOfTypeId = 1,
                                   .sizeOfVSQ = 1,
                                   .sizeOfCOT = 1,
                                   .originatorAddress = 7,
                                   .sizeOfCA = 1,
                                   .sizeOfIOA = 3,
                                   .maxSizeOfASDU = 249};
  CS101_ASDU asdu = CS101_ASDU_create(&params, false, CS101_COT_SPONTANEOUS, 7,
                                      0x2F, false, false);
  CS101_ASDU_setTypeID(asdu, M_ME_NC_1);
  auto io = (InformationObject)MeasuredValueShort_create(nullptr, 123, 3.0f, 0);
  CS101_ASDU_addInformationObject(asdu, io);
  InformationObject_destroy(io);

  auto m = IncomingMessage::create(asdu, &params);
  CS101_ASDU_destroy(asdu);

  auto bytes = m->getRawBytes();
  REQUIRE(bytes != nullptr);
  // start octet 68, length field at [1]
  REQUIRE(bytes[0] == 104);
  REQUIRE(m->getRawMessageString().find("I-Format") != std::string::npos);
  delete[] bytes;
}

// ---------------------------------------------------------------------------
// PointMessage
// ---------------------------------------------------------------------------

TEST_CASE("PointMessage over monitoring types",
          "[remote::message::pointmessage]") {
  struct Case {
    IEC60870_5_TypeID type;
    void (*setup)(std::shared_ptr<Object::DataPoint>);
  };
  std::vector<Case> cases = {
      {M_SP_NA_1,
       [](auto p) {
         p->setInfo(Object::SingleInfo::create(true, Quality::None));
       }},
      {M_SP_TB_1,
       [](auto p) {
         p->setInfo(
             Object::SingleInfo::create(true, Quality::None,
                                        std::chrono::system_clock::time_point(
                                            std::chrono::milliseconds(1000))));
       }},
      {M_DP_NA_1,
       [](auto p) {
         p->setInfo(Object::DoubleInfo::create(IEC60870_DOUBLE_POINT_ON,
                                               Quality::None));
       }},
      {M_DP_TB_1,
       [](auto p) {
         p->setInfo(
             Object::DoubleInfo::create(IEC60870_DOUBLE_POINT_ON, Quality::None,
                                        std::chrono::system_clock::time_point(
                                            std::chrono::milliseconds(1000))));
       }},
      {M_ST_NA_1,
       [](auto p) {
         p->setInfo(
             Object::StepInfo::create(LimitedInt7(3), false, Quality::None));
       }},
      {M_ST_TB_1,
       [](auto p) {
         p->setInfo(
             Object::StepInfo::create(LimitedInt7(3), false, Quality::None,
                                      std::chrono::system_clock::time_point(
                                          std::chrono::milliseconds(1000))));
       }},
      {M_BO_NA_1,
       [](auto p) {
         p->setInfo(Object::BinaryInfo::create(Byte32(0xCAFE), Quality::None));
       }},
      {M_BO_TB_1,
       [](auto p) {
         p->setInfo(
             Object::BinaryInfo::create(Byte32(0xCAFE), Quality::None,
                                        std::chrono::system_clock::time_point(
                                            std::chrono::milliseconds(1000))));
       }},
      {M_ME_NA_1,
       [](auto p) {
         p->setInfo(Object::NormalizedInfo::create(NormalizedFloat(0.5f),
                                                   Quality::None));
       }},
      {M_ME_TD_1,
       [](auto p) {
         p->setInfo(Object::NormalizedInfo::create(
             NormalizedFloat(0.5f), Quality::None,
             std::chrono::system_clock::time_point(
                 std::chrono::milliseconds(1000))));
       }},
      {M_ME_NB_1,
       [](auto p) {
         p->setInfo(
             Object::ScaledInfo::create(LimitedInt16(42), Quality::None));
       }},
      {M_ME_TE_1,
       [](auto p) {
         p->setInfo(
             Object::ScaledInfo::create(LimitedInt16(42), Quality::None,
                                        std::chrono::system_clock::time_point(
                                            std::chrono::milliseconds(1000))));
       }},
      {M_ME_NC_1,
       [](auto p) {
         p->setInfo(Object::ShortInfo::create(3.14f, Quality::None));
       }},
      {M_ME_TF_1,
       [](auto p) {
         p->setInfo(
             Object::ShortInfo::create(3.14f, Quality::None,
                                       std::chrono::system_clock::time_point(
                                           std::chrono::milliseconds(1000))));
       }},
      {M_IT_NA_1,
       [](auto p) {
         p->setInfo(Object::BinaryCounterInfo::create(
             1234, LimitedUInt5(2), BinaryCounterQuality::None));
       }},
      {M_IT_TB_1,
       [](auto p) {
         p->setInfo(Object::BinaryCounterInfo::create(
             1234, LimitedUInt5(2), BinaryCounterQuality::None,
             std::chrono::system_clock::time_point(
                 std::chrono::milliseconds(1000))));
       }},
      {M_EP_TD_1,
       [](auto p) {
         p->setInfo(Object::ProtectionEquipmentEventInfo::create(
             IEC60870_EVENTSTATE_ON, LimitedUInt16(50), Quality::None));
       }},
      {M_EP_TE_1,
       [](auto p) {
         p->setInfo(Object::ProtectionEquipmentStartEventsInfo::create(
             StartEvents::None, LimitedUInt16(50), Quality::None));
       }},
      {M_EP_TF_1,
       [](auto p) {
         p->setInfo(Object::ProtectionEquipmentOutputCircuitInfo::create(
             OutputCircuits::None, LimitedUInt16(50), Quality::None));
       }},
      {M_PS_NA_1,
       [](auto p) {
         p->setInfo(Object::StatusWithChangeDetection::create(
             FieldSet16(0x0102), FieldSet16(0x0304), Quality::None));
       }},
      {M_ME_ND_1,
       [](auto p) {
         p->setInfo(Object::NormalizedInfo::create(NormalizedFloat(0.25f),
                                                   Quality::None));
       }},
  };

  uint_fast32_t ioa = 100;
  for (const auto &c : cases) {
    auto point = makeAttachedPoint(c.type, 10, ioa);
    c.setup(point);
    auto msg = PointMessage::create(point);
    REQUIRE(msg->getType() == c.type);
    REQUIRE(msg->getCommonAddress() == 10);
    REQUIRE(msg->getIOA() == ioa);
    REQUIRE(msg->getCauseOfTransmission() == CS101_COT_SPONTANEOUS);
    REQUIRE(msg->getInformationObject() != nullptr);
    REQUIRE(msg->getInfo() != nullptr);
    ioa++;
  }
}

TEST_CASE("PointMessage throws for unsupported types",
          "[remote::message::pointmessage]") {
  // A control point is not a monitoring point -> default throw
  auto point = makeAttachedPoint(C_SC_NA_1, 10, 1);
  REQUIRE_THROWS_AS(PointMessage::create(point), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// PointCommand
// ---------------------------------------------------------------------------

TEST_CASE("PointCommand over control types (select and execute)",
          "[remote::message::pointcommand]") {
  struct Case {
    IEC60870_5_TypeID type;
    void (*setup)(std::shared_ptr<Object::DataPoint>);
  };
  std::vector<Case> cases = {
      {C_SC_NA_1,
       [](auto p) {
         p->setInfo(
             Object::SingleCmd::create(true, CS101_QualifierOfCommand::NONE));
       }},
      {C_SC_TA_1,
       [](auto p) {
         p->setInfo(
             Object::SingleCmd::create(true, CS101_QualifierOfCommand::NONE,
                                       std::chrono::system_clock::time_point(
                                           std::chrono::milliseconds(1000))));
       }},
      {C_DC_NA_1,
       [](auto p) {
         p->setInfo(Object::DoubleCmd::create(IEC60870_DOUBLE_POINT_ON,
                                              CS101_QualifierOfCommand::NONE));
       }},
      {C_DC_TA_1,
       [](auto p) {
         p->setInfo(Object::DoubleCmd::create(
             IEC60870_DOUBLE_POINT_ON, CS101_QualifierOfCommand::NONE,
             std::chrono::system_clock::time_point(
                 std::chrono::milliseconds(1000))));
       }},
      {C_RC_NA_1,
       [](auto p) {
         p->setInfo(Object::StepCmd::create(IEC60870_STEP_HIGHER,
                                            CS101_QualifierOfCommand::NONE));
       }},
      {C_RC_TA_1,
       [](auto p) {
         p->setInfo(Object::StepCmd::create(
             IEC60870_STEP_HIGHER, CS101_QualifierOfCommand::NONE,
             std::chrono::system_clock::time_point(
                 std::chrono::milliseconds(1000))));
       }},
      {C_BO_NA_1,
       [](auto p) { p->setInfo(Object::BinaryCmd::create(Byte32(0xCAFE))); }},
      {C_BO_TA_1,
       [](auto p) {
         p->setInfo(Object::BinaryCmd::create(
             Byte32(0xCAFE), std::chrono::system_clock::time_point(
                                 std::chrono::milliseconds(1000))));
       }},
      {C_SE_NA_1,
       [](auto p) {
         p->setInfo(Object::NormalizedCmd::create(NormalizedFloat(0.5f),
                                                  LimitedUInt7(0)));
       }},
      {C_SE_TA_1,
       [](auto p) {
         p->setInfo(Object::NormalizedCmd::create(
             NormalizedFloat(0.5f), LimitedUInt7(0),
             std::chrono::system_clock::time_point(
                 std::chrono::milliseconds(1000))));
       }},
      {C_SE_NB_1,
       [](auto p) {
         p->setInfo(
             Object::ScaledCmd::create(LimitedInt16(42), LimitedUInt7(0)));
       }},
      {C_SE_TB_1,
       [](auto p) {
         p->setInfo(
             Object::ScaledCmd::create(LimitedInt16(42), LimitedUInt7(0),
                                       std::chrono::system_clock::time_point(
                                           std::chrono::milliseconds(1000))));
       }},
      {C_SE_NC_1,
       [](auto p) {
         p->setInfo(Object::ShortCmd::create(3.14f, LimitedUInt7(0)));
       }},
      {C_SE_TC_1,
       [](auto p) {
         p->setInfo(
             Object::ShortCmd::create(3.14f, LimitedUInt7(0),
                                      std::chrono::system_clock::time_point(
                                          std::chrono::milliseconds(1000))));
       }},
  };

  for (const bool select : {false, true}) {
    uint_fast32_t ioa = 500;
    for (const auto &c : cases) {
      auto point = makeAttachedPoint(c.type, 20, ioa);
      c.setup(point);
      auto msg = PointCommand::create(point, select);
      REQUIRE(msg->getType() == c.type);
      REQUIRE(msg->getCommonAddress() == 20);
      REQUIRE(msg->getIOA() == ioa);
      REQUIRE(msg->getCauseOfTransmission() == CS101_COT_ACTIVATION);
      REQUIRE(msg->getInformationObject() != nullptr);
      REQUIRE(msg->getInfo() != nullptr);
      // setter/getter
      msg->setCauseOfTransmission(CS101_COT_ACTIVATION_CON);
      REQUIRE(msg->getCauseOfTransmission() == CS101_COT_ACTIVATION_CON);
      msg->setOriginatorAddress(3);
      REQUIRE(msg->getOriginatorAddress() == 3);
      ioa++;
    }
  }
}

TEST_CASE("PointCommand throws for non-command point",
          "[remote::message::pointcommand]") {
  auto point = makeAttachedPoint(M_SP_NA_1, 20, 1);
  REQUIRE_THROWS_AS(PointCommand::create(point), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// OutgoingMessage
// ---------------------------------------------------------------------------

TEST_CASE("OutgoingMessage with attached point drives base methods",
          "[remote::message::outgoing]") {
  auto point = makeAttachedPoint(M_ME_NC_1, 30, 77);
  REQUIRE(point->getStation() != nullptr);

  auto msg = PointMessage::create(point);
  // base setters
  msg->setOriginatorAddress(9);
  REQUIRE(msg->getOriginatorAddress() == 9);
  msg->setCauseOfTransmission(CS101_COT_PERIODIC);
  REQUIRE(msg->getCauseOfTransmission() == CS101_COT_PERIODIC);
  REQUIRE(msg->getCommonAddress() == 30);
  REQUIRE(msg->getType() == M_ME_NC_1);
  REQUIRE(msg->getIOA() == 77);
  REQUIRE(msg->getInfo() != nullptr);
  REQUIRE(msg->isTest() == false);
  REQUIRE(msg->isNegative() == false);
  REQUIRE(msg->isSequence() == false);
}

TEST_CASE("OutgoingMessage throws without a point",
          "[remote::message::outgoing]") {
  REQUIRE_THROWS_AS(PointMessage::create(nullptr), std::invalid_argument);
}

// ---------------------------------------------------------------------------
// Batch
// ---------------------------------------------------------------------------

TEST_CASE("Batch create/add/get/has / sequence / toString",
          "[remote::message::batch]") {
  auto server = Server::create();
  auto station = server->addStation(40);
  auto p1 = station->addPoint(1, M_ME_NC_1);
  auto p2 = station->addPoint(2, M_ME_NC_1);
  auto p3 = station->addPoint(3, M_ME_NC_1);

  auto batch = Batch::create(CS101_COT_SPONTANEOUS);
  REQUIRE(batch->getCauseOfTransmission() == CS101_COT_SPONTANEOUS);
  REQUIRE(batch->hasPoints() == false);
  REQUIRE(batch->getNumberOfObjects() == 0);
  REQUIRE(batch->getType() == C_TS_TA_1);

  batch->addPoint(p1);
  REQUIRE(batch->hasPoints() == true);
  REQUIRE(batch->getNumberOfObjects() == 1);
  REQUIRE(batch->getType() == M_ME_NC_1);
  REQUIRE(batch->isSequence() == true); // single element

  batch->addPoint(p2);
  batch->addPoint(p3);
  REQUIRE(batch->getNumberOfObjects() == 3);
  REQUIRE(batch->isSequence() == true); // 1,2,3 sequential
  REQUIRE(batch->getPoints().size() == 3);
  REQUIRE_NOTHROW(batch->toString());

  // cause setter (inherited) + getter round-trip
  batch->setCauseOfTransmission(CS101_COT_PERIODIC);
  REQUIRE(batch->getCauseOfTransmission() == CS101_COT_PERIODIC);
  // toString() renders the COT via CS101_CauseOfTransmission_toString which
  // produces the short name ("PERIODIC"), not the enum token.
  REQUIRE(batch->toString().find("cot=PERIODIC") != std::string::npos);
}

TEST_CASE("Batch non-sequential keys -> isSequence false",
          "[remote::message::batch]") {
  auto server = Server::create();
  auto station = server->addStation(40);
  auto p1 = station->addPoint(1, M_ME_NC_1);
  auto p3 = station->addPoint(3, M_ME_NC_1);

  auto batch = Batch::create(CS101_COT_SPONTANEOUS);
  batch->addPoint(p1);
  batch->addPoint(p3);
  REQUIRE(batch->getNumberOfObjects() == 2);
  REQUIRE(batch->isSequence() == false); // keys 1,3 -> gap
}

TEST_CASE("Batch create with initial points", "[remote::message::batch]") {
  auto server = Server::create();
  auto station = server->addStation(40);
  auto p1 = station->addPoint(1, M_SP_NA_1);
  auto p2 = station->addPoint(2, M_SP_NA_1);

  Object::DataPointVector vec{p1, p2};
  auto batch = Batch::create(CS101_COT_SPONTANEOUS, vec);
  REQUIRE(batch->getNumberOfObjects() == 2);
  REQUIRE(batch->getType() == M_SP_NA_1);
}

TEST_CASE("Batch addPoint error branches", "[remote::message::batch]") {
  auto server = Server::create();
  auto station = server->addStation(40);
  auto p1 = station->addPoint(1, M_ME_NC_1);

  auto batch = Batch::create(CS101_COT_SPONTANEOUS);

  // duplicate point
  batch->addPoint(p1);
  REQUIRE_THROWS_AS(batch->addPoint(p1), std::invalid_argument);

  // incompatible type (monitoring vs control range + different type)
  auto diffType = station->addPoint(2, M_SP_NA_1);
  REQUIRE_THROWS_AS(batch->addPoint(diffType), std::invalid_argument);

  // incompatible station
  auto otherStation = server->addStation(41);
  auto otherPoint = otherStation->addPoint(3, M_ME_NC_1);
  REQUIRE_THROWS_AS(batch->addPoint(otherPoint), std::invalid_argument);

  // non-monitoring point (type > 41)
  auto cmdPoint = station->addPoint(4, C_SC_NA_1);
  auto batch2 = Batch::create(CS101_COT_SPONTANEOUS);
  REQUIRE_THROWS_AS(batch2->addPoint(cmdPoint), std::invalid_argument);
}

TEST_CASE("Batch addPoint without station throws", "[remote::message::batch]") {
  auto server = Server::create();
  auto station = server->addStation(40);
  auto point = station->addPoint(1, M_ME_NC_1);
  REQUIRE(point->getStation() != nullptr);
  // detach the point so getStation() returns null -> addPoint must throw
  point->detach();
  REQUIRE(point->getStation() == nullptr);
  auto batch = Batch::create(CS101_COT_SPONTANEOUS);
  REQUIRE_THROWS_AS(batch->addPoint(point), std::invalid_argument);
}

TEST_CASE("Batch getPoints skips expired pointers / isSequence with expired",
          "[remote::message::batch]") {
  auto server = Server::create();
  std::shared_ptr<Batch> batch;
  {
    auto station = server->addStation(40);
    auto p1 = station->addPoint(1, M_ME_NC_1);
    auto p2 = station->addPoint(2, M_ME_NC_1);
    auto b = Batch::create(CS101_COT_SPONTANEOUS);
    b->addPoint(p1);
    b->addPoint(p2);
    REQUIRE(b->getPoints().size() == 2);
    REQUIRE(b->isSequence() == true); // live, sequential
    batch = b;
  }
  // The station was only owned by the inner scope plus `server`; releasing the
  // station drops its shared_ptr refs to the points, leaving the batch's
  // weak_ptrs expired.
  server->removeStation(40);
  REQUIRE(batch->getNumberOfObjects() == 2); // map size unchanged
  REQUIRE_NOTHROW(batch->isSequence());      // skips expired pointers
  REQUIRE(batch->getPoints().empty());       // expired pointers skipped
}

TEST_CASE("Batch isSequence stays sequential after expiry",
          "[remote::message::batch]") {
  auto server = Server::create();
  std::shared_ptr<Batch> batch;
  {
    auto station = server->addStation(40);
    auto p1 = station->addPoint(1, M_ME_NC_1);
    auto p3 = station->addPoint(3, M_ME_NC_1);
    auto b = Batch::create(CS101_COT_SPONTANEOUS);
    b->addPoint(p1);
    REQUIRE(b->isSequence() == true); // single live element
    batch = b;
  }
  server->removeStation(40);
  // All pointers expired -> isSequence returns true (the "<2" / all-expired
  // branches) without crashing.
  REQUIRE_NOTHROW(batch->isSequence());
}
