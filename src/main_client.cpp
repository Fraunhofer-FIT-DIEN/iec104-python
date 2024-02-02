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

void cl_dump(std::shared_ptr<Client> my_client,
             std::shared_ptr<Remote::Connection> cl_connection_1) {
  if (cl_connection_1->isOpen()) {
    std::cout << std::endl;
    auto cl_ct_count = my_client->getConnections().size();
    std::cout << "CL] |--+ CLIENT has " << std::to_string(cl_ct_count)
              << " connections" << std::endl;

    for (auto &ct_iter : my_client->getConnections()) {
      auto ct_st_count = ct_iter->getStations().size();
      std::cout << "       |--+ CONNECTION has " << std::to_string(ct_st_count)
                << " stations" << std::endl;

      for (auto &st_iter : ct_iter->getStations()) {
        auto st_pt_count = st_iter->getPoints().size();
        std::cout << "          |--+ STATION "
                  << std::to_string(st_iter->getCommonAddress()) << " has "
                  << std::to_string(st_pt_count) << " points" << std::endl;
        std::cout << "             |   TYPE         |    IOA     |        "
                     "VALUE         |      UPDATED AT      |      REPORTED AT  "
                     "   |      QUALITY      "
                  << std::endl;
        std::cout
            << "             "
               "|----------------|------------|----------------------|---------"
               "-------------|----------------------|-------------------"
            << std::endl;

        for (auto &pt_iter : st_iter->getPoints()) {
          std::cout << "             | " << TypeID_toString(pt_iter->getType())
                    << " | " << std::setw(10)
                    << std::to_string(pt_iter->getInformationObjectAddress())
                    << " | " << std::setw(20)
                    << std::to_string(pt_iter->getValue()) << " | "
                    << std::setw(20)
                    << std::to_string(pt_iter->getUpdatedAt_ms()) << " | "
                    << std::setw(20)
                    << std::to_string(pt_iter->getReportedAt_ms()) << " | "
                    << Quality_toString(pt_iter->getQuality()) << std::endl;
        }
        std::cout
            << "             "
               "|----------------|------------|----------------------|---------"
               "-------------|----------------------|-------------------"
            << std::endl;
      }
    }
  }
}

int main(int argc, char *argv[]) {
  py::scoped_interpreter guard{};
  bool const USE_TLS = true;
  std::string ROOT = argv[0];

  bool found = false;
  while (!found) {
    size_t index;
    index = ROOT.find_last_of("/\\");
    std::string const basename = ROOT.substr(index + 1);
    ROOT = ROOT.substr(0, index);
    if (ROOT.empty() || basename.find("cmake") != std::string::npos) {
      found = true;
    }
  }
  ROOT = ROOT + "/tests/";

  setDebug(Debug::Client | Debug::Connection);
  std::cout << "CL] DEBUG MODE: " << Debug_toString(getDebug()) << std::endl;

  std::shared_ptr<Remote::TransportSecurity> tlsconf{nullptr};

  if (USE_TLS) {
    tlsconf = Remote::TransportSecurity::create(true, true);
    tlsconf->setCertificate(ROOT + "certs/client1.crt",
                            ROOT + "certs/client1.key");
    tlsconf->setCACertificate(ROOT + "certs/ca.crt");
    tlsconf->setVersion(TLS_VERSION_TLS_1_2, TLS_VERSION_TLS_1_2);
    tlsconf->addAllowedRemoteCertificate(ROOT + "certs/server1.crt");
  }

  auto my_client = Client::create(1000, 5000, tlsconf);
  my_client->setOriginatorAddress(123);

  auto cl_connection_1 = my_client->addConnection("127.0.0.1", 19998);

  auto cl_station_1 = cl_connection_1->addStation(47);
  auto cl_step_command = cl_station_1->addPoint(32, C_RC_TA_1);
  cl_step_command->setValue(IEC60870_STEP_HIGHER);

  /*
   * connect loop
   */

  my_client->start();

  while (!cl_connection_1->isOpen()) {
    std::cout << "Waiting for connection" << std::endl;
    std::this_thread::sleep_for(1s);
  }

  auto cl_station_2 = cl_connection_1->addStation(47);
  if (!cl_station_2) {
    cl_station_2 = cl_connection_1->getStation(47);
    std::cout << "CL] Station with common address "
              << std::to_string(cl_station_2->getCommonAddress())
              << " already exists" << std::endl;
  }

  auto cl_step_point = cl_station_2->addPoint(31, M_ST_TB_1);
  if (!cl_step_point) {
    cl_step_point = cl_station_2->getPoint(31);
    std::cout << "CL] Point with io address "
              << std::to_string(cl_step_point->getInformationObjectAddress())
              << " already exists from interrogation" << std::endl;
  }

  auto cl_measurement_point = cl_station_2->getPoint(11);
  std::this_thread::sleep_for(1s);

  /*
   * send single commands
   */

  auto cl_single_command = cl_station_2->addPoint(16, C_SC_NA_1);
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

  /*
   * send double commands
   */

  auto cl_double_command = cl_station_2->addPoint(22, C_DC_TA_1);

  cl_double_command->setValue(IEC60870_DOUBLE_POINT_ON);
  if (cl_double_command->transmit(CS101_COT_ACTIVATION)) {
    std::cout << "CL] transmit: Double command ON successful" << std::endl;
  } else {
    std::cout << "CL] transmit: Double command ON failed" << std::endl;
  }
  std::this_thread::sleep_for(1s);

  cl_double_command->setValue(IEC60870_DOUBLE_POINT_OFF);
  if (cl_double_command->transmit()) {
    std::cout << "CL] transmit: Double command OFF successful" << std::endl;
  } else {
    std::cout << "CL] transmit: Double command OFF failed" << std::endl;
  }
  std::this_thread::sleep_for(1s);

  /*
   * send set_point commands
   */

  auto cl_setpoint_1 = cl_station_2->addPoint(12, C_SE_NC_1);
  auto cl_setpoint_2 = cl_station_2->addPoint(13, C_SE_NC_1);

  cl_setpoint_1->setValue(13.45);
  if (cl_setpoint_1->transmit(CS101_COT_ACTIVATION)) {
    std::cout << "CL] transmit: Setpoint1 command successful" << std::endl;
  } else {
    std::cout << "CL] transmit: Setpoint1 command failed" << std::endl;
  }
  std::this_thread::sleep_for(1s);

  cl_setpoint_2->setValue(13.45);
  if (cl_setpoint_2->transmit()) {
    std::cout << "CL] transmit: Setpoint2 command successful" << std::endl;
  } else {
    std::cout << "CL] transmit: Setpoint2 command failed" << std::endl;
  }
  std::this_thread::sleep_for(1s);

  /*
   * loop through points
   */

  while (cl_connection_1->isOpen()) {
    cl_dump(my_client, cl_connection_1);

    if (cl_step_point->read()) {
      std::cout << "CL] read: command successful" << std::endl;
    } else {
      std::cout << "CL] read: command failed" << std::endl;
    }
    std::this_thread::sleep_for(3s);

    if (cl_step_command) {
      if (cl_step_command->transmit()) {
        std::cout << "CL] transmit: Step command successful" << std::endl;
      } else {
        std::cout << "CL] transmit: Step command failed" << std::endl;

        // fix station
        auto step_command_t = cl_station_2->addPoint(32, C_RC_TA_1);
        if (step_command_t) {
          cl_step_command = step_command_t;
        }
        cl_step_command->setValue(IEC60870_STEP_HIGHER);
      }
    }

    std::this_thread::sleep_for(3s);
  }

  /*
   * done
   */

  my_client->stop();

  return 0;
}
