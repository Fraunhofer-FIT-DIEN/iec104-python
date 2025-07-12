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
 * @file ClientPy.cpp
 * @brief python binding for Client class
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

#include "Client.h"
#include "module/Tuple.h"

using namespace pybind11::literals;

void init_client(py::module_ &m) {

  py::class_<Client, std::shared_ptr<Client>>(
      m, "Client",
      "This class represents a local client and provides access to meta "
      "information and connected remote servers")
      .def(
          py::init(&Client::create),
          R"def(__init__(self: c104.Client, tick_rate_ms: int = 100, command_timeout_ms: int = 10000, transport_security: c104.TransportSecurity = None) -> None

create a new 104er client

Parameters
----------
tick_rate_ms : int
    client thread update interval
command_timeout_ms : int
    time to wait for a command response
transport_security : c104.TransportSecurity, optional
    TLS configuration object

Example
-------
>>> my_client = c104.Client(tick_rate_ms=100, command_timeout_ms=10000)
)def",
          "tick_rate_ms"_a = 100, "command_timeout_ms"_a = 10000,
          "transport_security"_a = py::none())
      .def_property_readonly(
          "tick_rate_ms", &Client::getTickRate_ms,
          "int: the clients tick rate in milliseconds (read-only)")
      .def_property_readonly("is_running", &Client::isRunning,
                             "bool: test if client is running (read-only)")
      .def_property_readonly("has_connections", &Client::hasConnections,
                             "bool: test if client has at least one remote "
                             "server connection (read-only)")
      .def_property_readonly(
          "has_open_connections", &Client::hasOpenConnections,
          "bool: test if client has open connections to servers (read-only)")
      .def_property_readonly("open_connection_count",
                             &Client::getOpenConnectionCount,
                             "int: represents the number of open connections "
                             "to servers (read-only)")
      .def_property_readonly("has_active_connections",
                             &Client::hasActiveConnections,
                             "bool: test if client has active (open and not "
                             "muted) connections to servers (read-only)")
      .def_property_readonly("active_connection_count",
                             &Client::getActiveConnectionCount,
                             "int: get number of active (open and not muted) "
                             "connections to servers (read-only)")
      .def_property_readonly(
          "connections",
          [](const Client &c) {
            return Module::vector_to_tuple(c.getConnections());
          },
          "tuple[c104.Connection]: list of all remote terminal unit "
          "(server) Connection objects (read-only)")
      .def_property("originator_address", &Client::getOriginatorAddress,
                    &Client::setOriginatorAddress,
                    "int: originator address of this client (0-255)",
                    py::return_value_policy::copy)
      .def("start", &Client::start, R"def(start(self: c104.Client) -> None

start client and connect all connections

Example
-------
>>> my_client.start()
)def")
      .def("stop", &Client::stop, R"def(stop(self: c104.Client) -> None

disconnect all connections and stop client

Example
-------
>>> my_client.stop()
)def")
      .def(
          "add_connection", &Client::addConnection,
          R"def(add_connection(self: c104.Client, ip: str, port: int = 2404, init = c104.Init.ALL) -> c104.Connection | None

add a new remote server connection to this client and return the new connection object

Parameters
----------
ip: str
    remote terminal units ip address
port: int
    remote terminal units port
init: c104.Init
    communication initiation commands

Returns
-------
c104.Connection, optional
    connection object, if added, else None

Raises
------
ValueError
    ip or port are invalid

Example
-------
>>> con = my_client.add_connection(ip="192.168.50.3", port=2406, init=c104.Init.ALL)
)def",
          "ip"_a, "port"_a = IEC_60870_5_104_DEFAULT_PORT, "init"_a = INIT_ALL)
      .def(
          "get_connection",
          [](Client &client, const std::string ip, const int port,
             const int common_address) {
            if (!ip.empty() && common_address == 0) {
              return client.getConnection(ip, port);
            }
            if (ip.empty() && common_address > 0) {
              return client.getConnectionFromCommonAddress(common_address);
            }
            throw std::invalid_argument("either keyword arguments ip and port "
                                        "or common_address must be specified");
          },
          R"def(get_connection(self: c104.Client, ip: str = "", port: int = 2404, common_address: int = 0) -> c104.Connection | None

get a connection (either by ip and port or by common_address)

Parameters
----------
ip: str, optional
    remote terminal units ip address
port: int, optional
    remote terminal units port
common_address: int, optional
    common address (value between 1 and 65534)

Returns
-------
c104.Connection, optional
    connection object, if found else None

Example
-------
>>> conA = my_client.get_connection(ip="192.168.50.3")
>>> conB = my_client.get_connection(ip="192.168.50.3", port=2406)
>>> conC = my_client.get_connection(common_address=4711)
)def",
          "ip"_a = "", "port"_a = IEC_60870_5_104_DEFAULT_PORT,
          "common_address"_a = 0)
      .def("reconnect_all", &Client::reconnectAll,
           R"def(reconnect_all(self: c104.Client) -> None

close and reopen all connections

Example
-------
>>> my_client.reconnect_all()
)def")
      .def("disconnect_all", &Client::disconnectAll,
           R"def(disconnect_all(self: c104.Client) -> None

close all connections

Example
-------
>>> my_client.disconnect_all()
)def")
      .def(
          "on_station_initialized", &Client::setOnEndOfInitializationCallback,
          R"def(on_station_initialized(self: c104.Client, callable: collections.abc.Callable[[c104.Client, c104.Station, c104.Coi], None]) -> None

set python callback that will be executed on incoming end of initialization message from stations

Parameters
----------
callable: collections.abc.Callable[[c104.Client, c104.Station, c104.Coi], None]
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
--------------------
client: c104.Client
    client instance
station: c104.Station
    reporting station
cause: c104.Coi
    what caused the (re-)initialization procedure

Callable Returns
-----------------
None

Example
-------
>>> def cl_on_station_initialized(client: c104.Client, station: c104.Station, cause: c104.Coi) -> None:
>>>     print("STATION {0} INITIALIZED due to {1} | CLIENT OA {2}".format(station.common_address, cause, client.originator_address))
>>>
>>> my_client.on_station_initialized(callable=cl_on_station_initialized)
)def",
          "callable"_a)
      .def(
          "on_new_station", &Client::setOnNewStationCallback,
          R"def(on_new_station(self: c104.Client, callable: collections.abc.Callable[[c104.Client, c104.Connection, int], None]) -> None

set python callback that will be executed on incoming message from unknown station

Parameters
----------
callable: collections.abc.Callable[[c104.Client, c104.Connection, int], None]
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
--------------------
client: c104.Client
    client instance
connection: c104.Connection
    connection reporting station
common_address: int
    station common address (value between 1 and 65534)

Callable Returns
-----------------
None

Example
-------
>>> def cl_on_new_station(client: c104.Client, connection: c104.Connection, common_address: int) -> None:
>>>     print("NEW STATION {0} | CLIENT OA {1}".format(common_address, client.originator_address))
>>>     connection.add_station(common_address=common_address)
>>>
>>> my_client.on_new_station(callable=cl_on_new_station)
)def",
          "callable"_a)
      .def(
          "on_new_point", &Client::setOnNewPointCallback,
          R"def(on_new_point(self: c104.Client, callable: collections.abc.Callable[[c104.Client, c104.Station, int, c104.Type], None]) -> None

set python callback that will be executed on incoming message from unknown point

Parameters
----------
callable: collections.abc.Callable[[c104.Client, c104.Station, int, c104.Type], None]
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
client: c104.Client
    client instance
station: c104.Station
    station reporting point
io_address: int
    point information object address (value between 0 and 16777215)
point_type: c104.Type
    point information type

Callable Returns
----------------
None

Example
-------
>>> def cl_on_new_point(client: c104.Client, station: c104.Station, io_address: int, point_type: c104.Type) -> None:
>>>     print("NEW POINT: {1} with IOA {0} | CLIENT OA {2}".format(io_address, point_type, client.originator_address))
>>>     point = station.add_point(io_address=io_address, type=point_type)
>>>
>>> my_client.on_new_point(callable=cl_on_new_point)
)def",
          "callable"_a)
      .def("__repr__", &Client::toString);
}
