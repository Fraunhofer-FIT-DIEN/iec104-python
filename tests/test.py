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

print("-"*60)
print("- RUN: TEST")
print("-"*60)

c104.set_debug_mode(mode=c104.Debug())
print("DEBUG MODE: {0}".format(c104.get_debug_mode()))

##################################
# CLIENT
##################################

my_client = c104.Client(tick_rate_ms=1000, command_timeout_ms=5000)
my_client.originator_address = 123

cl_connection_1 = my_client.add_connection(ip="127.0.0.1", port=2404)


def cl_pt_on_receive_point(point: c104.Point, previous_state: dict, message: c104.IncomingMessage) -> c104.ResponseState:
    print("CL] {0} REPORT on IOA: {1} , new: {2}, prev: {3}, cot: {4}, quality: {5}".format(point.type, point.io_address, point.value, previous_state, message.cot, point.quality))
    # print("{0}".format(message.is_negative))
    # print("-->| POINT: 0x{0} | EXPLAIN: {1}".format(message.raw.hex(), c104.explain_bytes(apdu=message.raw)))
    return c104.ResponseState.SUCCESS


##################################
# CLIENT: NEW HANDLER
##################################

def cl_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
    print("CL] NEW STATION {0} | CLIENT OA {1}".format(common_address, client.originator_address))
    connection.add_station(common_address=common_address)


def cl_on_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
    print("CL] NEW POINT: {1} with IOA {0} | CLIENT OA {2}".format(io_address, point_type, client.originator_address))
    point = station.add_point(io_address=io_address, type=point_type)
    point.on_receive(callable=cl_pt_on_receive_point)


my_client.on_new_station(callable=cl_on_new_station)
my_client.on_new_point(callable=cl_on_new_point)


##################################
# CLIENT: RAW MESSAGE HANDLER
##################################

def cl_ct_on_receive_raw(connection: c104.Connection, data: bytes) -> None:
    print("CL] -->| {1} [{0}] | CONN OA {2}".format(data.hex(), c104.explain_bytes_dict(apdu=data), connection.originator_address))


def cl_ct_on_send_raw(connection: c104.Connection, data: bytes) -> None:
    print("CL] <--| {1} [{0}] | CONN OA {2}".format(data.hex(), c104.explain_bytes_dict(apdu=data), connection.originator_address))


cl_connection_1.on_receive_raw(callable=cl_ct_on_receive_raw)
cl_connection_1.on_send_raw(callable=cl_ct_on_send_raw)

##################################
# CLIENT: Step command point
##################################

# cl_station_1 = connection_1.add_station(common_address=47)
cl_station_1 = cl_connection_1.add_station(common_address=46)  # use wrong common_address to test unexpected message error in server
cl_step_command = cl_station_1.add_point(io_address=32, type=c104.Type.C_RC_TA_1)
cl_step_command.value = c104.Step.HIGHER


def cl_dump():
    global my_client,cl_connection_1
    if cl_connection_1.is_connected:
        print("")
        cl_ct_count = len(my_client.connections)
        print("CL] |--+ CLIENT has {0} connections".format(cl_ct_count))
        for ct_iter in range(cl_ct_count):
            ct = my_client.connections[ct_iter]
            ct_st_count = len(ct.stations)
            print("       |--+ CONNECTION has {0} stations".format(ct_st_count))
            for st_iter in range(ct_st_count):
                st = ct.stations[st_iter]
                st_pt_count = len(st.points)
                print("          |--+ STATION {0} has {1} points".format(st.common_address, st_pt_count))
                print("             |   TYPE         |    IOA     |        VALUE         |      UPDATED AT      |      REPORTED AT     |      QUALITY      ")
                print("             |----------------|------------|----------------------|----------------------|----------------------|-------------------")
                for pt_iter in range(st_pt_count):
                    pt = st.points[pt_iter]
                    print("             | {0} | {1:10} | {2:20} | {3:20} | {4:20} | {5}".format(pt.type, pt.io_address, pt.value, pt.updated_at_ms,
                                                                                                pt.reported_at_ms, pt.quality))
                    print("             |----------------|------------|----------------------|----------------------|----------------------|-------------------")


##################################
# SERVER
##################################

my_server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=2000, max_connections=10)
my_server.max_connections = 11

sv_station_2 = my_server.add_station(common_address=47)


##################################
# SERVER: RAW MESSAGE HANDLER
##################################

def sv_on_connect(server: c104.Server, ip: str) -> bool:
    print("SV] <->| {0} | SERVER {1}:{2}".format(ip, server.ip, server.port))
    return ip == "127.0.0.1"


def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
    print("SV] -->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))


def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
    print("SV] <--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))


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
# SERVER: MEASUREMENT POINT WITH COMMAND
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


# Nan in short measurement value
sv_nan_point = sv_station_2.add_point(io_address=87, type=c104.Type.C_SE_NC_1)
sv_nan_point.value = float("NaN")

sv_measurement_point = sv_station_2.add_point(io_address=11, type=c104.Type.M_ME_NC_1, report_ms=1000)
sv_measurement_point.value = 12.34

sv_measurement_setpoint = sv_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1, report_ms=0, related_io_address=sv_measurement_point.io_address, related_io_autoreturn=True)
sv_measurement_setpoint.on_receive(callable=sv_pt_on_setpoint_command)

sv_measurement_setpoint_2 = sv_station_2.add_point(io_address=13, type=c104.Type.C_SE_NC_1)
sv_measurement_setpoint_2.related_io_address = 13  # use invalid IOA for testing purpose
sv_measurement_setpoint_2.related_io_autoreturn = False
sv_measurement_setpoint_2.on_receive(callable=sv_pt_on_setpoint_command)


##################################
# SERVER: DOUBLE POINT WITH COMMAND
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
# SERVER: STEP POINT WITH COMMAND
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

my_client.start()
my_server.start()

while not cl_connection_1.is_connected:
    print("CL] Try to connect to {0}:{1}".format(cl_connection_1.ip, cl_connection_1.port))
    cl_connection_1.connect()
    time.sleep(3)

cl_dump()
time.sleep(3)
print("-"*60)

##################################
# Add a method callback at runtime and change values
##################################

class PointMethodCallbackTestClass:
    def __init__(self, x: str):
        self.x = x

    def pt_on_before_auto_transmit_measurement_point(self, point: c104.Point) -> None:
        print(self.x, "--> {0} AUTO TRANSMIT on IOA: {1}".format(point.type, point.io_address))

pmct = PointMethodCallbackTestClass("SV] CALLBACK METHOD")

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

sv_measurement_point.value = 1234
sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)
sv_measurement_point.on_before_auto_transmit(callable=pmct.pt_on_before_auto_transmit_measurement_point)

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

sv_measurement_point.set(value=-1234.56, quality=c104.Quality.Invalid, timestamp_ms=int(time.time() * 1000))
sv_measurement_point.transmit(cause=c104.Cot.SPONTANEOUS)

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

cl_station_2 = cl_connection_1.add_station(common_address=47)
if cl_station_2 is None:
    cl_station_2 = cl_connection_1.get_station(common_address=47)
    print("CL] Station with common address {} already exists".format(cl_station_2.common_address))

cl_step_point = cl_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1)
if cl_step_point is None:
    cl_step_point = cl_station_2.get_point(io_address=31)
    print("CL] Point with io address {} already exists from interrogation".format(cl_step_point.io_address))

cl_nan_command = cl_station_2.add_point(io_address=87, type=c104.Type.C_SE_NC_1)
cl_nan_command.value = float("NaN")
if cl_nan_command.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Nan in short command successful")
else:
    print("CL] transmit: Nan in short command failed/invalid")

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

if cl_step_point.read():
    print("CL] read: command successful")
else:
    print("CL] read: command failed")

time.sleep(3)
print("-"*60)

cl_step_point.on_receive(callable=cl_pt_on_receive_point)

if cl_step_command.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Step command successful")
else:
    print("CL] transmit: Step command failed/invalid")
    if cl_station_2:
        # fix station
        cl_step_command_t = cl_station_2.add_point(io_address=32, type=c104.Type.C_RC_TA_1)
        if cl_step_command_t:
            cl_step_command = cl_step_command_t
            cl_step_command.value = c104.Step.HIGHER

        if cl_step_command.transmit(cause=c104.Cot.ACTIVATION):
            print("CL] transmit: Step command successful/fixed")

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

cl_measurement_point = cl_station_2.get_point(io_address=11)
if cl_measurement_point:
    cl_measurement_point.on_receive(callable=cl_pt_on_receive_point)

##################################
# double command
##################################

cl_double_command = cl_station_2.add_point(io_address=22, type=c104.Type.C_DC_TA_1)
cl_double_command.value = c104.Double.ON
if cl_double_command.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Double command ON successful")
else:
    print("CL] transmit: Double command ON failed")

time.sleep(3)

cl_double_command.value = c104.Double.OFF
if cl_double_command.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Double command OFF successful")
else:
    print("CL] transmit: Double command OFF failed")

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

##################################
# setpoint command
##################################

cl_setpoint_1 = cl_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1)
cl_setpoint_2 = cl_station_2.add_point(io_address=13, type=c104.Type.C_SE_NC_1)

cl_setpoint_1.value = 54.32
if cl_setpoint_1.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Setpoint command successful")
else:
    print("CL] transmit: Setpoint command failed")

time.sleep(3)
print("-"*60)

cl_setpoint_2.value = 11.11
if cl_setpoint_2.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Setpoint command successful")
else:
    print("CL] transmit: Setpoint command failed")

time.sleep(3)
cl_dump()
time.sleep(3)
print("-"*60)

##################################
# done
##################################

my_server.stop()
my_client.stop()

my_server = None
my_client = None

print("-"*60)
print("- FINISHED: TEST")
print("-"*60)
