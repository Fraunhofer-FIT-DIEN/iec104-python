#!/usr/bin/env python3

"""
 Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology FIT

 This file is part of iec104-python.
 iec104-python is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 iec104-python is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with iec104-python. If not, see <https://www.gnu.org/licenses/>.

 See LICENSE file for the complete license text.
"""

import c104
import time
import datetime
from pathlib import Path

USE_TLS = True
ROOT = Path(__file__).absolute().parent

c104.set_debug_mode(mode=c104.Debug.Server)
print("SV] DEBUG MODE: {0}".format(c104.get_debug_mode()))

if USE_TLS:
    tlsconf = c104.TransportSecurity(validate=True, only_known=True)
    tlsconf.set_certificate(cert=str(ROOT / "certs/server.crt"), key=str(ROOT / "certs/server.key"))
    tlsconf.set_ca_certificate(cert=str(ROOT / "certs/ca.crt"))
    tlsconf.set_version(min=c104.TlsVersion.TLS_1_2, max=c104.TlsVersion.TLS_1_2)
    tlsconf.add_allowed_remote_certificate(cert=str(ROOT / "certs/client1.crt"))
    my_server = c104.Server(ip="0.0.0.0", port=19998, tick_rate_ms=2000, max_connections=10, transport_security=tlsconf)
else:
    my_server = c104.Server(ip="0.0.0.0", port=19998, tick_rate_ms=2000, max_connections=10)

my_server.max_connections = 11

sv_station_2 = my_server.add_station(common_address=47)


##################################
# RAW MESSAGE HANDLER
##################################

def sv_on_connect(server: c104.Server, ip: str) -> bool:
    print("SV] <->| {0} | SERVER {1}:{2}".format(ip, server.ip, server.port))
    return ip == "127.0.0.1"


def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))


def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
    print("SV] <--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes_dict(apdu=data), server.ip, server.port))


def sv_on_clock_sync(server: c104.Server, ip: str, date_time: datetime.datetime) -> c104.ResponseState:
    print("SV] ->@| Time {0} from {1} | SERVER {2}:{3}".format(date_time, ip, server.ip, server.port))
    return c104.ResponseState.SUCCESS


def sv_on_unexpected_message(server: c104.Server, message: c104.IncomingMessage, cause: c104.Umc) -> None:
    print("SV] ->?| {1} from CLIENT OA {0} | SERVER {2}:{3}".format(message.originator_address, cause, server.ip, server.port))


my_server.on_receive_raw(callable=sv_on_receive_raw)
my_server.on_send_raw(callable=sv_on_send_raw)
my_server.on_connect(callable=sv_on_connect)
my_server.on_clock_sync(callable=sv_on_clock_sync)
my_server.on_unexpected_message(callable=sv_on_unexpected_message)


##################################
# MEASUREMENT POINT WITH COMMAND
##################################

def sv_pt_on_setpoint_command(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState:
    print("SV] {0} SETPOINT COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_state, message.cot, point.quality))

    if point.quality.is_good():
        if point.related_io_address:
            print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
            related_point = sv_station_2.get_point(point.related_io_address)
            if related_point:
                print("SV] -> RELATED POINT VALUE UPDATE")
                related_point.value = point.value
            else:
                print("SV] -> RELATED POINT NOT FOUND!")
        return c104.ResponseState.SUCCESS

    return c104.ResponseState.FAILURE


sv_measurement_point = sv_station_2.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
sv_measurement_point.value = 12.34

sv_measurement_setpoint = sv_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=sv_measurement_point.io_address, related_io_autoreturn=True)
sv_measurement_setpoint.on_receive(callable=sv_pt_on_setpoint_command)

sv_measurement_setpoint_2 = sv_station_2.add_point(io_address=13, type=c104.Type.C_SE_NC_1)
sv_measurement_setpoint_2.related_io_address = 14  # use invalid IOA for testing purpose
sv_measurement_setpoint_2.related_io_autoreturn = False
sv_measurement_setpoint_2.on_receive(callable=sv_pt_on_setpoint_command)


##################################
# DOUBLE POINT WITH COMMAND
##################################

def sv_pt_on_double_command(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState:
    print("SV] {0} DOUBLE COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_state, message.cot, point.quality))

    if point.quality.is_good():
        if point.related_io_address:
            print("SV] -> RELATED IO ADDRESS: {}".format(point.related_io_address))
            related_point = sv_station_2.get_point(point.related_io_address)
            if related_point:
                print("SV] -> RELATED POINT VALUE UPDATE")
                related_point.value = point.value
            else:
                print("SV] -> RELATED POINT NOT FOUND!")
        return c104.ResponseState.SUCCESS

    return c104.ResponseState.FAILURE


sv_double_point = sv_station_2.add_point(io_address=21, type=c104.Type.M_DP_TB_1)
sv_double_point.value = c104.Double.OFF
sv_double_point.report_ms = 4000

sv_double_command = sv_station_2.add_point(io_address=22, type=c104.Type.C_DC_TA_1, report_ms=0, related_io_address=sv_double_point.io_address, related_io_autoreturn=True)
sv_double_command.on_receive(callable=sv_pt_on_double_command)

##################################
# STEP POINT WITH COMMAND
##################################

sv_global_step_point_value = 0


def sv_pt_on_step_command(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState:
    global sv_global_step_point_value
    print("SV] {0} STEP COMMAND on IOA: {1}, new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_state, message.cot, point.quality))

    if point.value == c104.Step.LOWER:
        sv_global_step_point_value -= 1
        return c104.ResponseState.SUCCESS

    if point.value == c104.Step.HIGHER:
        sv_global_step_point_value += 1
        return c104.ResponseState.SUCCESS

    return c104.ResponseState.FAILURE


def sv_pt_on_before_read_step_point(point: c104.Point) -> None:
    global sv_global_step_point_value
    print("SV] {0} READ COMMAND on IOA: {1}".format(point.type, point.io_address))
    point.value = sv_global_step_point_value


def sv_pt_on_before_auto_transmit_step_point(point: c104.Point) -> None:
    global sv_global_step_point_value
    print("SV] {0} AUTO TRANSMIT on IOA: {1}".format(point.type, point.io_address))
    point.value = sv_global_step_point_value


sv_step_point = sv_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1)
sv_step_point.value = sv_global_step_point_value
sv_step_point.report_ms = 2000
sv_step_point.on_before_read(callable=sv_pt_on_before_read_step_point)
sv_step_point.on_before_auto_transmit(callable=sv_pt_on_before_auto_transmit_step_point)

sv_step_command = sv_station_2.add_point(io_address=32, type=c104.Type.C_RC_TA_1, report_ms=0, related_io_address=sv_step_point.io_address, related_io_autoreturn=True)
sv_step_command.on_receive(callable=sv_pt_on_step_command)

##################################
# RUN
##################################

input("Press Enter to start server...")
my_server.start()


class ServerPointMethodCallbackTestClass:
    def __init__(self, x: str):
        self.x = x

    def pt_on_before_auto_transmit_measurement_point(self, point: c104.Point) -> None:
        print(self.x, "--> {0} AUTO TRANSMIT on IOA: {1}".format(point.type, point.io_address))

sv_pmct = ServerPointMethodCallbackTestClass("SV] CALLBACK METHOD")

time.sleep(5)
sv_measurement_point.value = 1234
sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)
sv_measurement_point.on_before_auto_transmit(callable=sv_pmct.pt_on_before_auto_transmit_measurement_point)

time.sleep(5)
sv_measurement_point.set(value=-1234.56, quality=c104.Quality.Invalid, timestamp_ms=int(time.time() * 1000))
sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)

##################################
# done
##################################

input("Press Enter to stop server...")
my_server.stop()
