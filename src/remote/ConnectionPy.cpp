/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file ConnectionPy.cpp
 * @brief python binding for Connection class
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "module/Tuple.h"
#include "remote/Connection.h"

using namespace pybind11::literals;

void init_remote_connection(py::module_ &m) {

  py::class_<Remote::Connection, std::shared_ptr<Remote::Connection>>(
      m, "Connection",
      "This class represents connections from a client to a remote server and "
      "provides access to meta information and containing stations")
      .def_property_readonly(
          "ip", &Remote::Connection::getIP,
          "str: remote terminal units (server) ip (read-only)")
      .def_property_readonly(
          "port", &Remote::Connection::getPort,
          "int: remote terminal units (server) port (read-only)")
      .def_property_readonly(
          "state", &Remote::Connection::getState,
          "c104.ConnectionState: current connection state (read-only)")
      .def_property_readonly(
          "has_stations", &Remote::Connection::hasStations,
          "bool: test if remote server has at least one station (read-only)")
      .def_property_readonly(
          "stations",
          [](const Remote::Connection &c) {
            return Module::vector_to_tuple(c.getStations());
          },
          "tuple[c104.Station]: list of all Station objects (read-only)")
      .def_property_readonly("is_connected", &Remote::Connection::isOpen,
                             "bool: test if connection is opened (read-only)")
      .def_property_readonly("is_muted", &Remote::Connection::isMuted,
                             "bool: test if connection is muted (read-only)")
      .def_property("originator_address",
                    &Remote::Connection::getOriginatorAddress,
                    &Remote::Connection::setOriginatorAddress,
                    "int: originator address of this connection (0-255)")
      .def_property_readonly(
          "connected_at", &Remote::Connection::getConnectedAt,
          "datetime.datetime | None : datetime of last connection opening, if "
          "connection is open (read-only)")
      .def_property_readonly(
          "disconnected_at", &Remote::Connection::getDisconnectedAt,
          "datetime.datetime | None : datetime of last connection closing, if "
          "connection is closed (read-only)")
      .def_property_readonly(
          "protocol_parameters", &Remote::Connection::getParameters,
          "c104.ProtocolParameters: read and update protocol parameters",
          py::return_value_policy::reference)
      .def("connect", &Remote::Connection::connect,
           R"def(connect(self: c104.Connection) -> None

initiate connection to remote terminal unit (server) in a background thread (non-blocking)

Example
-------
>>> my_connection.connect()
)def")
      .def("disconnect", &Remote::Connection::disconnect,
           R"def(disconnect(self: c104.Connection) -> None

close connection to remote terminal unit (server)

Example
-------
>>> my_connection.disconnect()
)def")
      .def("mute", &Remote::Connection::mute,
           R"def(mute(self: c104.Connection) -> bool

tell the remote terminal unit (server) that this connection is muted, prohibit monitoring messages

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.mute():
>>>     raise ValueError("Cannot mute connection")
)def",
           py::return_value_policy::copy)
      .def("unmute", &Remote::Connection::unmute,
           R"def(unmute(self: c104.Connection) -> bool

tell the remote terminal unit (server) that this connection is not muted, allow monitoring messages

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.unmute():
>>>     raise ValueError("Cannot unmute connection")
)def",
           py::return_value_policy::copy)
      .def(
          "interrogation", &Remote::Connection::interrogation,
          R"def(interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Qoi = c104.Qoi.STATION, wait_for_response: bool = True) -> bool

send an interrogation command to the remote terminal unit (server)

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
cause: c104.Cot
    cause of transmission
qualifier: c104.Qoi
    qualifier of interrogation
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Raises
------
ValueError
    qualifier is invalid

Example
-------
>>> if not my_connection.interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Qoi.STATION):
>>>     raise ValueError("Cannot send interrogation command")
)def",
          "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
          "qualifier"_a = QOI_STATION, "wait_for_response"_a = true,
          py::return_value_policy::copy)
      .def(
          "counter_interrogation", &Remote::Connection::counterInterrogation,
          R"def(counter_interrogation(self: c104.Connection, common_address: int, cause: c104.Cot = c104.Cot.ACTIVATION, qualifier: c104.Rqt = c104.Rqt.GENERAL, freeze: c104.Frz = c104.Frz.READ, wait_for_response: bool = True) -> bool

send a counter interrogation command to the remote terminal unit (server)

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
cause: c104.Cot
    cause of transmission
qualifier: c104.Rqt
    what counters are addressed
freeze: c104.Frz
    counter behaviour
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.counter_interrogation(common_address=47, cause=c104.Cot.ACTIVATION, qualifier=c104.Rqt.GENERAL, freeze=c104.Frz.COUNTER_RESET):
>>>     raise ValueError("Cannot send counter interrogation command")
)def",
          "common_address"_a, "cause"_a = CS101_COT_ACTIVATION,
          "qualifier"_a = CS101_QualifierOfCounterInterrogation::GENERAL,
          "freeze"_a = CS101_FreezeOfCounterInterrogation::READ,
          "wait_for_response"_a = true, py::return_value_policy::copy)
      .def(
          "clock_sync", &Remote::Connection::clockSync,
          R"def(clock_sync(self: c104.Connection, common_address: int, date_time: c104.DateTime, wait_for_response: bool = True) -> bool

send a clock synchronization command to the remote terminal unit (server)
the clients OS time is used

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
date_time: c104.DateTime
    to be sent timestamp
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.clock_sync(common_address=47):
>>>     raise ValueError("Cannot send clock sync command")
)def",
          "common_address"_a, "date_time"_a = Object::DateTime::now(),
          "wait_for_response"_a = true, py::return_value_policy::copy)
      .def(
          "test", &Remote::Connection::test,
          R"def(test(self: c104.Connection, common_address: int, with_time: bool = True, wait_for_response: bool = True) -> bool

send a test command to the remote terminal unit (server)
the clients OS time is used

Parameters
----------
common_address: int
    station common address (The valid range is 0 to 65535. Using the values 0 or 65535 sends the command to all stations, acting as a wildcard.)
with_time: bool
    send with or without timestamp
wait_for_response: bool
    block call until command success or failure response received?

Returns
-------
bool
    True, if connection is Open, False otherwise

Example
-------
>>> if not my_connection.test(common_address=47):
>>>     raise ValueError("Cannot send test command")
)def",
          "common_address"_a, "with_time"_a = true,
          "wait_for_response"_a = true, py::return_value_policy::copy)
      .def(
          "add_station", &Remote::Connection::addStation,
          R"def(add_station(self: c104.Connection, common_address: int) -> c104.Station | None

add a new station to this connection and return the new station object

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
c104.Station, optional
    station object, if station was added, else None

Example
-------
>>> station_1 = my_connection.add_station(common_address=15)
)def",
          "common_address"_a)
      .def(
          "get_station", &Remote::Connection::getStation,
          R"def(get_station(self: c104.Connection, common_address: int) -> c104.Station | None

get a station object via common address

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
c104.Station, optional
    station object, if found, else None

Example
-------
>>> station_14 = my_connection.get_station(common_address=14)
)def",
          "common_address"_a)
      .def(
          "remove_station", &Remote::Connection::removeStation,
          R"def(remove_station(self: c104.Connection, common_address: int) -> bool

removes an existing station from this connection

Parameters
----------
common_address: int
    station common address (value between 1 and 65534)

Returns
-------
bool
    True if the station was successfully removed, otherwise False.

Example
-------
>>> my_connection.remove_station(common_address=12)
)def",
          "common_address"_a)
      .def(
          "on_receive_raw", &Remote::Connection::setOnReceiveRawCallback,
          R"def(on_receive_raw(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, bytes], None]) -> None

set python callback that will be executed on incoming message

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, bytes], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def con_on_receive_raw(connection: c104.Connection, data: bytes) -> None:
>>>     print("-->| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
>>>
>>> my_connection.on_receive_raw(callable=con_on_receive_raw)
)def",
          "callable"_a)
      .def(
          "on_send_raw", &Remote::Connection::setOnSendRawCallback,
          R"def(on_send_raw(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, bytes], None]) -> None

set python callback that will be executed on outgoing message

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, bytes], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def con_on_send_raw(connection: c104.Connection, data: bytes) -> None:
>>>     print("<--| {1} [{0}] | CON {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), connection.ip, connection.port))
>>>
>>> my_connection.on_send_raw(callable=con_on_send_raw)
)def",
          "callable"_a)
      .def(
          "on_state_change", &Remote::Connection::setOnStateChangeCallback,
          R"def(on_state_change(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, c104.ConnectionState], None]) -> None

set python callback that will be executed on connection state changes

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, c104.ConnectionState], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
state: c104.ConnectionState
    latest connection state

Callable Returns
----------------
None

Example
-------
>>> def con_on_state_change(connection: c104.Connection, state: c104.ConnectionState) -> None:
>>>     print("CON {0}:{1} STATE changed to {2}".format(connection.ip, connection.port, state))
>>>
>>> my_connection.on_state_change(callable=con_on_state_change)
)def",
          "callable"_a)
      .def(
          "on_unexpected_message",
          &Remote::Connection::setOnUnexpectedMessageCallback,
          R"def(on_unexpected_message(self: c104.Connection, callable: collections.abc.Callable[[c104.Connection, c104.IncomingMessage, c104.Umc], None]) -> None

set python callback that will be executed on unexpected incoming messages

Parameters
----------
callable: collections.abc.Callable[[c104.Connection, c104.IncomingMessage, c104.Umc], None]
    callback function reference

Returns
-------
None

Raises
------
ValueError
    callable signature does not match exactly

**Callable signature**

Callable Parameters
-------------------
connection: c104.Connection
    connection instance
message: c104.IncomingMessage
    incoming message
cause: c104.Umc
    unexpected message cause

Callable Returns
----------------
None

Example
-------
>>> def con_on_unexpected_message(connection: c104.Connection, message: c104.IncomingMessage, cause: c104.Umc) -> None:
>>>     print("->?| {1} from STATION CA {0}".format(message.common_address, cause))
>>>
>>> my_connection.on_unexpected_message(callable=con_on_unexpected_message)
)def",
          "callable"_a)
      .def("__repr__", &Remote::Connection::toString);
  ;
}
