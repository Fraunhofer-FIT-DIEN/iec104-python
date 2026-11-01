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
 * @file mock_helpers.h
 * @brief Shared scaffolding for driving iec104-python C++ code to high line
 * coverage without live network traffic. Includes a mock sIPeerConnection
 * (IMasterConnection), helpers to build crafted CS101_ASDU structs and
 * DataPoints, and a convenience MockServer wrapper.
 *
 * NOTE ON GIL: methods that touch Module::ScopedGilRelease/ScopedGilAcquire or
 * any pybind11 callback require an embedded Python interpreter. Mirror
 * tests/main.cpp. Every test TU that uses these helpers must include
 * <pybind11/pybind11.h> and (like the existing tests) be linked into the
 * c104_tests executable whose main.cpp keeps a py::scoped_interpreter alive
 * for the whole process. Do NOT spawn a second scoped_interpreter.
 */
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/Connection.h"
#include "remote/Helper.h"
#include "remote/TransportSecurity.h"
#include "remote/message/IncomingMessage.h"
#include "remote/message/OutgoingMessage.h"
#include "remote/message/PointCommand.h"
#include "remote/message/PointMessage.h"
#include "types.h"

#include <pybind11/pybind11.h> // ensures embedding interpreter for GIL paths

namespace Test {

/**
 * Small mutable state captured by the mock sIPeerConnection lambdas so tests
 * can assert on what the code under test "sent back" to the peer.
 */
struct MockPeerState {
  int sendASDU_count = 0;
  int sendACT_CON_count = 0;
  int sendACT_TERM_count = 0;
  int close_count = 0;
  std::vector<int> send_ids; // CS101_ASDU_getID of every sendASDU call
  std::vector<int> oa;       // originator addresses pushed on sendASDU
  std::vector<int> ele;      // number of elements pushed on sendASDU
};

/**
 * Build a stack sIPeerConnection (IMasterConnection) wired to @p state.
 * The object field points at MockPeerState. This mirrors the proven pattern
 * from tests/test_server_interrogation.cpp.
 */
inline sIPeerConnection makeMockPeer(MockPeerState &state) {
  sIPeerConnection peer = {
      .isReady = [](IMasterConnection) { return true; },
      .sendASDU =
          [](IMasterConnection self, CS101_ASDU asdu) {
            auto *s = static_cast<MockPeerState *>(self->object);
            s->sendASDU_count++;
            (void)CS101_ASDU_getTypeID(asdu);
            s->oa.push_back(CS101_ASDU_getOA(asdu));
            s->ele.push_back(CS101_ASDU_getNumberOfElements(asdu));
            return true;
          },
      .sendACT_CON =
          [](IMasterConnection self, CS101_ASDU asdu, bool /*negative*/) {
            static_cast<MockPeerState *>(self->object)->sendACT_CON_count++;
            return true;
          },
      .sendACT_TERM =
          [](IMasterConnection self, CS101_ASDU asdu) {
            static_cast<MockPeerState *>(self->object)->sendACT_TERM_count++;
            return true;
          },
      .close =
          [](IMasterConnection self) {
            static_cast<MockPeerState *>(self->object)->close_count++;
          },
      .getPeerAddress =
          [](IMasterConnection self, char *addrBuf, int addrBufSize) {
            (void)self;
            std::string ip = "127.0.0.1";
            std::size_t n = ip.copy(addrBuf, addrBufSize - 1);
            addrBuf[n] = '\0';
            return 0;
          },
      .getApplicationLayerParameters =
          [](IMasterConnection self) {
            (void)self;
            return static_cast<CS101_AppLayerParameters>(nullptr);
          },
      .object = &state};
  return peer;
}

/**
 * Convenience default application layer parameters (as used across the repo's
 * own tests).
 */
inline sCS101_AppLayerParameters defaultAppLayerParameters() {
  return {.sizeOfTypeId = 1,
          .sizeOfVSQ = 1,
          .sizeOfCOT = 2,
          .originatorAddress = 0,
          .sizeOfCA = 2,
          .sizeOfIOA = 3,
          .maxSizeOfASDU = 249};
}

/**
 * Craft an ASDU of the given type with a single information object at @p ioa.
 * The information object itself is left as a plain InformationObject with no
 * added IO so callers can attach type-specific IOs or just exercise the
 * facade/metadata paths.
 *
 * @param type    IEC 60870 type id (will be written via CS101_ASDU_setTypeID)
 * @param ioa     information object address
 * @param cot     cause of transmission
 * @param oa      originator address
 * @param ca      common address
 * @param params  app layer parameters (nullptr -> default)
 */
inline CS101_ASDU makeASDU(IEC60870_5_TypeID type, uint_fast32_t ioa,
                           CS101_CauseOfTransmission cot, uint_fast8_t oa,
                           uint_fast16_t ca,
                           sCS101_AppLayerParameters *params = nullptr) {
  // CS101_ASDU stores a POINTER to the app-layer parameters and keeps using it
  // later (e.g. CS101_ASDU_clone / getCA during IncomingMessage creation), so
  // the default parameters MUST outlive the returned ASDU. A function-local
  // static lives for the whole process - safe. (A stack local would dangle the
  // instant makeASDU returned, causing a segfault when the ASDU is read later.)
  static sCS101_AppLayerParameters local = defaultAppLayerParameters();
  sCS101_AppLayerParameters *p = params ? params : &local;
  CS101_ASDU asdu = CS101_ASDU_create(p, false, cot, oa, ca, false, false);
  CS101_ASDU_setTypeID(asdu, type);
  CS101_ASDU_setCOT(asdu, cot);
  CS101_ASDU_setCA(asdu, ca);
  (void)ioa; // IO address is carried by the information object added by the
             // caller; there is no ASDU-level IOA setter in this lib60870 API
  return asdu;
}

/**
 * Create a standalone server-backed station with @p commonAddress and a single
 * DataPoint of @p type at @p ioa. Returns the point. The server is a plain
 * (non-started) Server used only for construction/ownership; it is left to the
 * caller to start/stop if a live socket is desired.
 */
inline std::shared_ptr<Object::DataPoint>
makeDataPoint(IEC60870_5_TypeID type, uint_fast16_t commonAddress,
              uint_fast32_t ioa) {
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(commonAddress);
  return station->addPoint(ioa, type);
}

} // namespace Test
