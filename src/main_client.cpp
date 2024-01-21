/**
 * Copyright 2020-2024 Fraunhofer Institute for Applied Information Technology
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
 *
 * @file main_client.cpp
 * @brief client debug executable
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Client.h"
#include <pybind11/embed.h> // everything needed for embedding
namespace py = pybind11;

int main(int argc, char *argv[]) {
  py::scoped_interpreter guard{};

  auto client = Client::create(1000, 1000);
  auto connection = client->addConnection("127.0.0.1", 19998);
  auto station = connection->addStation(47);

  auto point = station->addPoint(11, M_ME_NC_1);

  auto command = station->addPoint(12, C_RC_TA_1);
  command->setValue(IEC60870_STEP_HIGHER);

  setDebug(Debug::Client | Debug::Connection);

  client->start();

  while (true) {
    while (!connection->isOpen()) {
      std::cout << "Waiting for connection" << std::endl;
      std::this_thread::sleep_for(1s);
    }

    auto cl_single_command = station->addPoint(16, C_SC_NA_1);
    cl_single_command->setValue(0);
    if (cl_single_command->transmit()) {
      std::cout << "CL] transmit: Single command OFF successful" << std::endl;
    } else {
      std::cout << "CL] transmit: Single command OFF failed" << std::endl;
    }
    std::this_thread::sleep_for(1s);
    cl_single_command->setCommandMode(SELECT_AND_EXECUTE_COMMAND);
    if (cl_single_command->transmit()) {
      std::cout << "CL] transmit: Single command OFF successful" << std::endl;
    } else {
      std::cout << "CL] transmit: Single command OFF failed" << std::endl;
    }

    std::this_thread::sleep_for(1s);
    while (connection->getState() == OPEN) {
      std::cout << "READ" << std::endl;
      if (point->read()) {
        std::cout << "-> SUCCESS" << std::endl;
      } else {
        std::cout << "-> FAILURE" << std::endl;
      }
      std::this_thread::sleep_for(1s);
      std::cout << "COMMAND" << std::endl;
      if (command->transmit(CS101_COT_ACTIVATION)) {
        std::cout << "-> SUCCESS" << std::endl;
      } else {
        std::cout << "-> FAILURE" << std::endl;
      }
    }
  }

  return 0;
}
