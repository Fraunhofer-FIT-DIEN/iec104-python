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
 * @file ServerPy.cpp
 * @brief python binding for Server class
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <pybind11/chrono.h>
#include <pybind11/operators.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

#include "Server.h"
#include "module/Tuple.h"
#include "remote/message/Batch.h"

using namespace pybind11::literals;

void init_server(py::module_ &m) {

  py::class_<Server, std::shared_ptr<Server>>(
      m, "Server",
      "This class represents a local server and provides access to meta "
      "information and containing stations")
      .def(
          py::init(&Server::create),
          R"def(__init__(self: c104.Server, ip: str = "0.0.0.0", port: int = 2404, tick_rate_ms: int = 100, select_timeout_ms = 10000, max_connections: int = 0, transport_security: c104.TransportSecurity = None) -> None

create a new 104er server

Parameters
----------
ip: str
    listening server ip address
port:int
    listening server port
tick_rate_ms: int
    server thread update interval
select_timeout_ms: int
    execution for points in SELECT_AND_EXECUTE mode must arrive within this interval to succeed
max_connections: int
    maximum number of clients allowed to connect
transport_security: c104.TransportSecurity, optional
    TLS configuration object

Example
-------
>>> my_server = c104.Server(ip="0.0.0.0", port=2404, tick_rate_ms=100, select_timeout_ms=10000, max_connections=0)
)def",
          "ip"_a = "0.0.0.0", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
          "tick_rate_ms"_a = 100, "select_timeout_ms"_a = 10000,
          "max_connections"_a = 0, "transport_security"_a = py::none())
      .def_property_readonly(
          "tick_rate_ms", &Server::getTickRate_ms,
          "int: the servers tick rate in milliseconds (read-only)")
      .def_property_readonly("ip", &Server::getIP,
                             "str: ip address the server will accept "
                             "connections on, \"0.0.0.0\" = any (read-only)")
      .def_property_readonly(
          "port", &Server::getPort,
          "int: port number the server will accept connections on (read-only)")
      .def_property_readonly("is_running", &Server::isRunning,
                             "bool: test if server is running (read-only)")
      .def_property_readonly(
          "has_open_connections", &Server::hasOpenConnections,
          "bool: test if server has open connections to clients (read-only)")
      .def_property_readonly("open_connection_count",
                             &Server::getOpenConnectionCount,
                             "int: represents the number of open connections "
                             "to clients (read-only)")
      .def_property_readonly("has_active_connections",
                             &Server::hasActiveConnections,
                             "bool: test if server has active (open and not "
                             "muted) connections to clients (read-only)")
      .def_property_readonly("active_connection_count",
                             &Server::getActiveConnectionCount,
                             "int: get number of active (open and not muted) "
                             "connections to clients (read-only)")
      .def_property_readonly(
          "has_stations", &Server::hasStations,
          "bool: test if server has at least one station (read-only)")
      .def_property_readonly(
          "stations",
          [](const Server &s) {
            return Module::vector_to_tuple(s.getStations());
          },
          "tuple[c104.Station]: list of all local "
          "Station objects (read-only)")
      .def_property_readonly(
          "protocol_parameters", &Server::getParameters,
          "c104.ProtocolParameters: read and update protocol parameters",
          py::return_value_policy::reference)
      .def_property("max_connections", &Server::getMaxOpenConnections,
                    &Server::setMaxOpenConnections,
                    "int: maximum number of open connections, 0 = no limit",
                    py::return_value_policy::copy)
      .def("start", &Server::start, R"def(start(self: c104.Server) -> None

open local server socket for incoming connections

Raises
------
RuntimeError
    server thread failed to start

Example
-------
>>> my_server.start()
)def")
      .def("stop", &Server::stop, R"def(stop(self: c104.Server) -> None

stop local server socket

Example
-------
>>> my_server.stop()
)def")
      .def(
          "add_station", &Server::addStation,
          R"def(add_station(self: c104.Server, common_address: int) -> c104.Station | None

add a new station to this server and return the new station object

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
>>> station_1 = my_server.add_station(common_address=15)
)def",
          "common_address"_a)
      .def(
          "get_station", &Server::getStation,
          R"def(get_station(self: c104.Server, common_address: int) -> c104.Station | None

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
>>> station_2 = my_server.get_connection(common_address=14)
)def",
          "common_address"_a)
      .def("remove_station", &Server::removeStation,
           R"def(remove_station(self: c104.Server, common_address: int) -> bool

removes an existing station from this server

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
>>> station_3.remove_station(common_address=12)
)def",
           "common_address"_a)
      .def(
          "transmit_batch",
          [](Server &server, std::shared_ptr<Remote::Message::Batch> batch) {
            return server.sendBatch(std::move(batch));
          },
          R"def(transmit_batch(self: c104.Server, batch: c104.Batch) -> bool

transmit a batch object

Parameters
----------
batch: c104.Batch
    batch object to transmit

Returns
-------
bool
    send success

Example
-------
>>> success = my_server.transmit_batch(c104.Batch([point1, point2, point3]))
)def",
          "batch"_a)
      .def(
          "on_receive_raw", &Server::setOnReceiveRawCallback,
          R"def(on_receive_raw(self: c104.Server, callable: collections.abc.Callable[[c104.Server, bytes], None]) -> None

set python callback that will be executed on incoming message

Parameters
----------
callable: collections.abc.Callable[[c104.Server, bytes], None]
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
server: c104.Server
    server instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def sv_on_receive_raw(server: c104.Server, data: bytes) -> None:
>>>     print("-->| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
>>>
>>> my_server.on_receive_raw(callable=sv_on_receive_raw)
)def",
          "callable"_a)
      .def(
          "on_send_raw", &Server::setOnSendRawCallback,
          R"def(on_send_raw(self: c104.Server, callable: collections.abc.Callable[[c104.Server, bytes], None]) -> None

set python callback that will be executed on outgoing message

Parameters
----------
callable: collections.abc.Callable[[c104.Server, bytes], None]
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
server: c104.Server
    server instance
data: bytes
    raw message bytes

Callable Returns
----------------
None

Example
-------
>>> def sv_on_send_raw(server: c104.Server, data: bytes) -> None:
>>>     print("<--| {1} [{0}] | SERVER {2}:{3}".format(data.hex(), c104.explain_bytes(apdu=data), server.ip, server.port))
>>>
>>> my_server.on_send_raw(callable=sv_on_send_raw)
)def",
          "callable"_a)
      .def(
          "on_connect", &Server::setOnConnectCallback,
          R"def(on_connect(self: c104.Server, callable: collections.abc.Callable[[c104.Server, ip], bool]) -> None

set python callback that will be executed on incoming connection requests

Parameters
----------
callable: collections.abc.Callable[[c104.Server, ip], bool]
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
server: c104.Server
    server instance
ip: str
    client connection request ip

Callable Returns
----------------
bool
    accept or reject the connection request

Example
-------
>>> def sv_on_connect(server: c104.Server, ip: str) -> bool:
>>>     print("<->| {0} | SERVER {1}:{2}".format(ip, server.ip, server.port))
>>>     return ip == "127.0.0.1"
>>>
>>> my_server.on_connect(callable=sv_on_connect)
)def",
          "callable"_a)
      .def(
          "on_clock_sync", &Server::setOnClockSyncCallback,
          R"def(on_clock_sync(self: c104.Server, callable: collections.abc.Callable[[c104.Server, str, c104.DateTime], c104.ResponseState]) -> None

set python callback that will be executed on incoming clock sync command

Parameters
----------
callable: collections.abc.Callable[[c104.Server, str, c104.DateTime], c104.ResponseState]
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
server: c104.Server
    server instance
ip: str
    client connection request ip
date_time: c104.DateTime
    clients current clock time

Callable Returns
----------------
c104.ResponseState
    success or failure of clock sync command

Example
-------
>>> import datetime
>>>
>>> def sv_on_clock_sync(server: c104.Server, ip: str, date_time: c104.DateTime) -> c104.ResponseState:
>>>     print("->@| Time {0} from {1} | SERVER {2}:{3}".format(date_time, ip, server.ip, server.port))
>>>     return c104.ResponseState.SUCCESS
>>>
>>> my_server.on_clock_sync(callable=sv_on_clock_sync)
)def",
          "callable"_a)
      .def(
          "on_unexpected_message", &Server::setOnUnexpectedMessageCallback,
          R"def(on_unexpected_message(self: c104.Server, callable: collections.abc.Callable[[c104.Server, c104.IncomingMessage, c104.Umc], None]) -> None

set python callback that will be executed on unexpected incoming messages

Parameters
----------
callable: collections.abc.Callable[[c104.Server, c104.IncomingMessage, c104.Umc], None]
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
server: c104.Server
    server instance
message: c104.IncomingMessage
    incoming message
cause: c104.Umc
    unexpected message cause

Callable Returns
----------------
None

Example
-------
>>> def sv_on_unexpected_message(server: c104.Server, message: c104.IncomingMessage, cause: c104.Umc) -> None:
>>>     print("->?| {1} from CLIENT OA {0} | SERVER {2}:{3}".format(message.originator_address, cause, server.ip, server.port))
>>>
>>> my_server.on_unexpected_message(callable=sv_on_unexpected_message)
)def",
          "callable"_a)
      .def("__repr__", &Server::toString);
}
