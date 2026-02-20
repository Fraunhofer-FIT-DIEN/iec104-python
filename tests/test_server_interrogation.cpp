/**
 * Copyright 2026-2026 Fraunhofer Institute for Applied Information Technology
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
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include "Server.h"
#include "object/DataPoint.h"
#include "object/Station.h"
#include "remote/message/Batch.h"
#include "remote/message/IncomingMessage.h"
#include "types.h"

namespace py = pybind11;

struct MockData {
  int con;
  int term;
  int send;
  std::vector<int> oa;
  std::vector<int> ele;
};

TEST_CASE("Server interrogation originator address",
          "[server][interrogation]") {
  // setDebug(Debug::All ^ Debug::Gil);

  // Create a server
  auto server = Server::create("127.0.0.1", 2404);
  auto station = server->addStation(1);

  // Add a monitoring point to the station
  for (int i = 0; i < 60; i++) {
    station->addPoint(100 + i, IEC60870_5_TypeID::M_ME_NC_1);
  }
  server->start();

  // Create a mock IMasterConnection (which is a void*)
  MockData data = {0, 0, 0, {}, {}};

  // Create an interrogation ASDU
  sCS101_AppLayerParameters alParams = {.sizeOfTypeId = 1,
                                        .sizeOfVSQ = 1,
                                        .sizeOfCOT = 2,
                                        .originatorAddress = 0,
                                        .sizeOfCA = 2,
                                        .sizeOfIOA = 3,
                                        .maxSizeOfASDU = 249};

  sIMasterConnection iMasterConnection = {
      .isReady = [](IMasterConnection self) { return true; },
      .sendASDU =
          [](IMasterConnection self, CS101_ASDU asdu) {
            ((MockData *)self->object)->send++;
            ((MockData *)self->object)->oa.push_back(CS101_ASDU_getOA(asdu));
            ((MockData *)self->object)
                ->ele.push_back(CS101_ASDU_getNumberOfElements(asdu));
            return true;
          },
      .sendACT_CON =
          [](IMasterConnection self, CS101_ASDU asdu, bool negative) {
            ((MockData *)self->object)->con++;
            return true;
          },
      .sendACT_TERM =
          [](IMasterConnection self, CS101_ASDU asdu) {
            ((MockData *)self->object)->term++;
            return true;
          },
      .close = [](IMasterConnection self) {},
      .getPeerAddress =
          [](IMasterConnection self, char *addrBuf, int addrBufSize) {
            memcpy(addrBuf, "127.0.0.1", addrBufSize);
            return 0;
          },
      .getApplicationLayerParameters =
          [](IMasterConnection self) {
            return static_cast<CS101_AppLayerParameters>(nullptr);
          },
      .object = &data};
  IMasterConnection mockConnection = &iMasterConnection;

  CS101_ASDU interrogationAsdu = CS101_ASDU_create(
      &alParams, false, CS101_COT_ACTIVATION, 123, 1, false, false);

  // Create an IncomingMessage from the ASDU
  server->connectionEventHandler(server.get(), mockConnection,
                                 CS104_CON_EVENT_ACTIVATED);
  REQUIRE(server->isExistingConnection(mockConnection) == true);
  auto incomingMessage =
      server->getValidMessage(mockConnection, interrogationAsdu);
  REQUIRE(incomingMessage->getOriginatorAddress() == 123);

  // We need to pass the raw pointer of server to interrogationHandler
  server->interrogationHandler(server.get(), mockConnection, interrogationAsdu,
                               IEC60870_QOI_STATION);
  REQUIRE(data.con == 0);
  REQUIRE(data.term == 0);
  REQUIRE(data.send == 4); // con, batch1-part1, batch1-part2, term
  REQUIRE(data.oa ==
          std::vector<int>({123, 123, 123, 123})); // always expected OA
  REQUIRE(data.ele ==
          std::vector<int>({0, 48, 12, 0})); // expected number of elements
  // setDebug(Debug::None);
}
