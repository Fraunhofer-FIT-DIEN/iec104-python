#!/usr/bin/env python3

"""
 Copyright 2020-2024 Fraunhofer Institute for Applied Information Technology FIT

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

import functools
import time
import datetime
from pathlib import Path

import c104

USE_TLS = True
ROOT = Path(__file__).absolute().parent

c104.set_debug_mode(mode=c104.Debug.Client|c104.Debug.Connection)
print("CL] DEBUG MODE: {0}".format(c104.get_debug_mode()))

if USE_TLS:
    tlsconf = c104.TransportSecurity(validate=True, only_known=True)
    tlsconf.set_certificate(cert=str(ROOT / "certs/client1.crt"), key=str(ROOT / "certs/client1.key"))
    tlsconf.set_ca_certificate(cert=str(ROOT / "certs/ca.crt"))
    tlsconf.set_version(min=c104.TlsVersion.TLS_1_2, max=c104.TlsVersion.TLS_1_2)
    tlsconf.add_allowed_remote_certificate(cert=str(ROOT / "certs/server1.crt"))
    my_client = c104.Client(tick_rate_ms=100, command_timeout_ms=100, transport_security=tlsconf)
else:
    my_client = c104.Client(tick_rate_ms=100, command_timeout_ms=100)

my_client.originator_address = 123

cl_connection_1 = my_client.add_connection(ip="127.0.0.1", port=19998, init=c104.Init.INTERROGATION)


##################################
# CONNECTION STATE HANDLER
##################################

def cl_ct_on_state_change(connection: c104.Connection, state: c104.ConnectionState) -> None:
    print("CL] Connection State Changed {0} | State {1}".format(connection.originator_address, state))


cl_connection_1.on_state_change(callable=cl_ct_on_state_change)


##################################
# NEW DATA HANDLER
##################################

def cl_pt_on_receive_point(point: c104.Point, previous_info: c104.Information, message: c104.IncomingMessage) -> c104.ResponseState:
    print("CL] {0} REPORT on IOA: {1}, cot: {2}, previous: {3}, current: {4}".format(point.type, point.io_address, message.cot, previous_info, point.info))
    # print("{0}".format(message.is_negative))
    # print("-->| POINT: 0x{0} | EXPLAIN: {1}".format(message.raw.hex(), c104.explain_bytes(apdu=message.raw)))
    return c104.ResponseState.SUCCESS


##################################
# NEW OBJECT HANDLER
##################################

def cl_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int, custom_arg: str, y: str = "default value") -> None:
    print("CL] NEW STATION {0} | CLIENT OA {1}".format(common_address, client.originator_address))
    connection.add_station(common_address=common_address)


def cl_on_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
    print("CL] NEW POINT: {1} with IOA {0} | CLIENT OA {2}".format(io_address, point_type, client.originator_address))
    point = station.add_point(io_address=io_address, type=point_type)
    point.on_receive(callable=cl_pt_on_receive_point)


my_client.on_new_station(callable=functools.partial(cl_on_new_station, custom_arg="extra argument with default/bounded value passes signature check"))
my_client.on_new_point(callable=cl_on_new_point)


##################################
# RAW MESSAGE HANDLER
##################################

def cl_ct_on_receive_raw(connection: c104.Connection, data: bytes) -> None:
    print("CL] -->| {1} [{0}] | CONN OA {2}".format(data.hex(), c104.explain_bytes_dict(apdu=data), connection.originator_address))


def cl_ct_on_send_raw(connection: c104.Connection, data: bytes) -> None:
    print("CL] <--| {1} [{0}] | CONN OA {2}".format(data.hex(), c104.explain_bytes_dict(apdu=data), connection.originator_address))


cl_connection_1.on_receive_raw(callable=cl_ct_on_receive_raw)
cl_connection_1.on_send_raw(callable=cl_ct_on_send_raw)

##################################
# Step command point
##################################

# station_1 = connection_1.add_station(common_address=47)
cl_station_1 = cl_connection_1.add_station(common_address=46)  # use wrong common_address to test unexpected message error in server
cl_step_command = cl_station_1.add_point(io_address=32, type=c104.Type.C_RC_TA_1)
cl_step_command.value = c104.Step.HIGHER


def cl_dump():
    global my_client, cl_connection_1
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
                print("             |      TYPE      |   IOA   |       VALUE       | PROCESSED  AT |  RECORDED AT  |      QUALITY      ")
                print("             |----------------|---------|-------------------|---------------|---------------|-------------------")
                for pt_iter in range(st_pt_count):
                    pt = st.points[pt_iter]
                    print("             | {0} | {1:7} | {2:17} | {3:13} | {4:13} | {5}".format(pt.type, pt.io_address, str(pt.value), pt.recorded_at or 'N. A.',
                                                                                               pt.processed_at, pt.quality))
                    print("             |----------------|---------|-------------------|---------------|---------------|-------------------")


##################################
# connect loop
##################################

input("Press Enter to start client...")
my_client.start()

while not cl_connection_1.is_connected or cl_connection_1.is_muted:
    print("CL] Waiting for connection to {0}:{1}".format(cl_connection_1.ip, cl_connection_1.port))
    time.sleep(3)

# input("Press Enter to stop client...")
# my_client.stop()
# exit(0)


##################################
# adding an existing station causes None return value, get an existing station object via getStation
##################################


cl_station_2 = cl_connection_1.add_station(common_address=47)
if cl_station_2 is None:
    cl_station_2 = cl_connection_1.get_station(common_address=47)
    print("CL] Station with common address {} already exists".format(cl_station_2.common_address))

cl_step_point = cl_station_2.add_point(io_address=31, type=c104.Type.M_ST_TB_1)
if cl_step_point is None:
    cl_step_point = cl_station_2.get_point(io_address=31)
    print("CL] Point with io address {} already exists from interrogation".format(cl_step_point.io_address))

cl_step_point.on_receive(callable=cl_pt_on_receive_point)

cl_measurement_point = cl_station_2.get_point(io_address=11)
if cl_measurement_point:
    cl_measurement_point.on_receive(callable=cl_pt_on_receive_point)

time.sleep(1)

##################################
# Send single commands
##################################


cl_single_command = cl_station_2.add_point(io_address=16, type=c104.Type.C_SC_NA_1)
print(cl_single_command.command_mode)

cl_single_command.value = False
if cl_single_command.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Single command OFF successful")
else:
    print("CL] transmit: Single command OFF failed")

time.sleep(1)

cl_single_command.command_mode = c104.CommandMode.SELECT_AND_EXECUTE
print(cl_single_command.command_mode)

cl_single_command.value = False
if cl_single_command.transmit(cause=c104.Cot.ACTIVATION, qualifier=c104.Qoc.SHORT_PULSE):
    print("CL] transmit: Single command OFF successful")
else:
    print("CL] transmit: Single command OFF failed")

time.sleep(1)

##################################
# Send double commands
##################################

cl_double_command = cl_station_2.add_point(io_address=22, type=c104.Type.C_DC_TA_1)

cl_double_command.info = c104.DoubleCmd(state=c104.Double.ON, qualifier=c104.Qoc.SHORT_PULSE, recorded_at=datetime.datetime.fromtimestamp(1711111111.111))
if cl_double_command.transmit(cause=c104.Cot.ACTIVATION, qualifier=c104.Qoc.LONG_PULSE):
    print("CL] transmit: Double command ON successful")
else:
    print("CL] transmit: Double command ON failed")

time.sleep(1)

cl_double_command = c104.DoubleCmd(state=c104.Double.OFF, qualifier=c104.Qoc.PERSISTENT, recorded_at=datetime.datetime.fromtimestamp(1711111111.111))
if cl_double_command.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Double command OFF successful")
else:
    print("CL] transmit: Double command OFF failed")

time.sleep(1)

##################################
# Send set_point commands
##################################

cl_setpoint_1 = cl_station_2.add_point(io_address=12, type=c104.Type.C_SE_NC_1)
cl_setpoint_2 = cl_station_2.add_point(io_address=13, type=c104.Type.C_SE_NC_1)

cl_setpoint_1.value = 13.45
if cl_setpoint_1.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Setpoint 1 command successful")
else:
    print("CL] transmit: Setpoint 1 command failed")

time.sleep(1)

cl_setpoint_2.value = 13.45
if cl_setpoint_2.transmit(cause=c104.Cot.ACTIVATION):
    print("CL] transmit: Setpoint 2 command successful")
else:
    print("CL] transmit: Setpoint 2 command failed")

time.sleep(1)

##################################
# Loop through points
##################################

while cl_connection_1.is_connected:
    cl_dump()

    if cl_step_point.read():
        print("CL]  > read: command successful")
    else:
        print("CL]  > read: command failed")

    time.sleep(3)

    if cl_step_command:
        if cl_step_command.transmit(cause=c104.Cot.ACTIVATION, qualifier=c104.Qoc.CONTINUOUS):
            print("CL]  > transmit: Step command successful")
        else:
            print("CL]  > transmit: Step command failed")
            # fix station
            step_command_t = cl_station_2.add_point(io_address=32, type=c104.Type.C_RC_TA_1)
            if step_command_t:
                cl_step_command = step_command_t
                cl_step_command.value = c104.Step.HIGHER

        time.sleep(3)

##################################
# done
##################################

my_client.stop()
