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
 * @file main_server.cpp
 * @brief server debug executable
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Server.h"
#include <pybind11/embed.h> // everything needed for embedding

namespace py = pybind11;

int main(int argc, char *argv[]) {
  py::scoped_interpreter guard{};
  auto c104 = py::module_::import("c104");

  bool const USE_TLS = false;
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

  setDebug(Debug::Server | Debug::Point | Debug::Callback);
  std::cout << "SV] DEBUG MODE: " << Debug_toString(getDebug()) << std::endl;

  std::shared_ptr<Remote::TransportSecurity> tlsconf{nullptr};

  if (USE_TLS) {
    tlsconf = Remote::TransportSecurity::create(true, true);
    tlsconf->setCertificate(ROOT + "certs/server1.crt",
                            ROOT + "certs/server1.key");
    tlsconf->setCACertificate(ROOT + "certs/ca.crt");
    tlsconf->setVersion(TLS_VERSION_TLS_1_2, TLS_VERSION_TLS_1_2);
    tlsconf->addAllowedRemoteCertificate(ROOT + "certs/client1.crt");
  }

  auto my_server = Server::create("127.0.0.1", 2404, 1000, 0, tlsconf);

  auto sv_station_2 = my_server->addStation(47);

  auto sv_measurement_point = sv_station_2->addPoint(11, M_ME_TF_1, 1000);
  sv_measurement_point->setValue((float)12.34);

  auto sv_control_setpoint = sv_station_2->addPoint(
      12, C_SE_NC_1, 0, sv_measurement_point->getInformationObjectAddress(),
      true);

  // use invalid return IOA for testing purpose
  auto sv_control_setpoint_2 =
      sv_station_2->addPoint(13, C_SE_NC_1, 0, 14, false);

  auto sv_single_point = sv_station_2->addPoint(15, M_SP_NA_1);
  sv_single_point->setValue(true);

  auto sv_single_command = sv_station_2->addPoint(16, C_SC_NA_1, 0, 15, true,
                                                  SELECT_AND_EXECUTE_COMMAND);

  auto sv_double_point = sv_station_2->addPoint(21, M_DP_TB_1, 4000);
  sv_double_point->setValue(IEC60870_DOUBLE_POINT_OFF);

  auto sv_double_command = sv_station_2->addPoint(
      22, C_DC_TA_1, 0, sv_double_point->getInformationObjectAddress(), true);

  auto sv_step_point = sv_station_2->addPoint(31, M_ST_TB_1, 2000);
  sv_step_point->setValue(LimitedInt7(1));

  auto sv_step_command = sv_station_2->addPoint(
      32, C_RC_TA_1, 0, sv_step_point->getInformationObjectAddress(), true);

  auto locals = py::dict(
      "my_server"_a = my_server, "sv_control_setpoint"_a = sv_control_setpoint,
      "sv_control_setpoint_2"_a = sv_control_setpoint_2,
      "sv_single_command"_a = sv_single_command,
      "sv_double_command"_a = sv_double_command,
      "sv_step_point"_a = sv_step_point, "sv_step_command"_a = sv_step_command);

  try {
    py::exec(R"(
import c104

def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
    import c104
    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))

def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
    import c104
    print("SV] <--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))

def sv_pt_on_setpoint_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    import c104
    print("SV] {0} SETPOINT COMMAND on IOA: {1}, cot: {2}, previous: {3}, current: {4}".format(point.type, point.io_address, message.cot, previous_info, point.info))

    if point.related_io_address:
        print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
        related_point = point.station.get_point(point.related_io_address)
        if related_point:
            print("SV] -> RELATED POINT VALUE UPDATE")
            related_point.value = point.value
        else:
            print("SV] -> RELATED POINT NOT FOUND!")
    return c104.ResponseState.SUCCESS


def sv_pt_on_single_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    import c104
    print("SV] {0} SINGLE COMMAND on IOA: {1}, cot: {2}, previous: {3}, current: {4}".format(point.type, point.io_address, message.cot, previous_info, point.info))

    if message.is_select_command:
        print("SV] -> SELECTED BY: {}".format(point.selected_by))
    else:
        print("SV] -> EXECUTED BY {}, NEW SELECTED BY={}".format(message.originator_address, point.selected_by))
    return c104.ResponseState.SUCCESS


sv_global_step_point_value = c104.Int7(0)

def sv_pt_on_double_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    import c104
    print("SV] {0} DOUBLE COMMAND on IOA: {1}, cot: {2}, previous: {3}, current: {4}".format(point.type, point.io_address, message.cot, previous_info, point.info))

    if point.related_io_address:
        print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
        related_point = point.station.get_point(point.related_io_address)
        if related_point:
            print("SV] -> RELATED POINT VALUE UPDATE")
            related_point.value = point.value
        else:
            print("SV] -> RELATED POINT NOT FOUND!")
    return c104.ResponseState.SUCCESS

def sv_pt_on_step_command(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    import c104
    global sv_global_step_point_value
    print("SV] {0} STEP COMMAND on IOA: {1}, cot: {2}, previous: {3}, current: {4}".format(point.type, point.io_address, message.cot, previous_info, point.info))

    if point.value == c104.Step.LOWER:
        sv_global_step_point_value -= 1
        return c104.ResponseState.SUCCESS

    if point.value == c104.Step.HIGHER:
        sv_global_step_point_value += 1
        return c104.ResponseState.SUCCESS

    return c104.ResponseState.FAILURE


def sv_pt_on_before_transmit_step_point(point: c104.Point) -> None:
    import c104
    global sv_global_step_point_value
    print("SV] {0} READ COMMAND on IOA: {1}".format(point.type, point.io_address))
    point.value = sv_global_step_point_value


my_server.on_receive_raw(callable=sv_on_receive_raw)
my_server.on_send_raw(callable=sv_on_send_raw)
sv_control_setpoint.on_receive(callable=sv_pt_on_setpoint_command)
sv_control_setpoint_2.on_receive(callable=sv_pt_on_setpoint_command)
sv_single_command.on_receive(callable=sv_pt_on_single_command)
sv_double_command.on_receive(callable=sv_pt_on_double_command)
sv_step_point.on_before_read(callable=sv_pt_on_before_transmit_step_point)
sv_step_point.on_before_auto_transmit(callable=sv_pt_on_before_transmit_step_point)
sv_step_command.on_receive(callable=sv_pt_on_step_command)

)",
             py::globals(), locals);

  } catch (py::error_already_set &e) {
    std::cerr << '\n' << " Python Error:" << std::endl;

    auto traceback = py::module_::import("traceback");
    traceback.attr("print_exception")(e.type(), e.value(), e.trace());

    std::cerr << "------------------------------------------------------------"
              << '\n'
              << std::endl;
    return 1;
  }

  Module::ScopedGilRelease scoped("main");

  /*
   * connect loop
   */

  my_server->start();
  //  while(true) {
  //    my_server->start();
  //    my_server->stop();
  //  }

  while (!my_server->hasActiveConnections()) {
    std::cout << "Waiting for connection" << std::endl;
    std::this_thread::sleep_for(1s);
  }

  /*
   * test
   */

  std::this_thread::sleep_for(10s);

  sv_measurement_point->setInfo(
      Object::ShortInfo::create(1234, Quality::None, 1711111111111));
  if (sv_measurement_point->transmit(CS101_COT_SPONTANEOUS)) {
    std::cout << "SV] transmit: Measurement point send successful" << std::endl;
  } else {
    std::cout << "SV] transmit: Measurement point send failed" << std::endl;
  }

  std::this_thread::sleep_for(10s);

  sv_measurement_point->setInfo(
      Object::ShortInfo::create(-1234.56, Quality::Invalid, 1711111111111));
  if (sv_measurement_point->transmit(CS101_COT_SPONTANEOUS)) {
    std::cout << "SV] transmit: Measurement point send successful" << std::endl;
  } else {
    std::cout << "SV] transmit: Measurement point send failed" << std::endl;
  }

  /*
   * done
   */

  my_server->stop();

  return 0;
}
