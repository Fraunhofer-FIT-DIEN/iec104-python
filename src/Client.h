/**
 * Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology
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
 * @file Client.h
 * @brief operate a scada unit
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_CLIENT_H
#define C104_CLIENT_H

#include "types.h"

#include "module/Callback.h"
#include "module/GilAwareMutex.h"
#include "remote/Connection.h"
#include "remote/TransportSecurity.h"

/**
 * @brief service model for IEC60870-5-104 communication as client
 */
class Client : public std::enable_shared_from_this<Client> {
  // @todo import/export station with DataPoints to string
  // @todo add callback each packet
public:
  // noncopyable
  Client(const Client &) = delete;
  Client &operator=(const Client &) = delete;

  [[nodiscard]] static std::shared_ptr<Client> create(
      std::uint_fast32_t tick_rate_ms = 1000,
      std::uint_fast32_t timeout_ms = 1000,
      std::shared_ptr<Remote::TransportSecurity> transport_security = nullptr) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Client>(
        new Client(tick_rate_ms, timeout_ms, std::move(transport_security)));
  }

  /**
   * @brief Close and destroy all connections of this connection handler
   */
  ~Client();

  /**
   * @brief start reconnect thread
   */
  void start();

  /**
   * @brief stop reconnect thread
   */
  void stop();

  /**
   * @brief test if client is currently active
   * @return information on active state of client
   */
  bool isRunning();

  /**
   * @brief Sets the originator address of all client connections to the new
   * value if not changed per connection
   * @param address originator address of a client message
   */
  void setOriginatorAddress(std::uint_fast8_t address);

  /**
   * @brief Getter for originatorAddress: who is the originator of a client
   * message
   * @return originator address of a client message
   */
  std::uint_fast8_t getOriginatorAddress() const;

  // CONNECTION HANDLING

  bool hasConnections();

  Remote::ConnectionVector getConnections();

  bool hasConnection(const std::string &ip,
                     std::uint_fast16_t port = IEC_60870_5_104_DEFAULT_PORT);

  std::shared_ptr<Remote::Connection>
  getConnection(const std::string &ip,
                std::uint_fast16_t port = IEC_60870_5_104_DEFAULT_PORT);

  /**
   * @brief add a new remote server connection to this client and return the new
   * connection object
   * @param ip remote terminal units ip address
   * @param port remote terminal units port
   * @param init communication initiation commands
   * @return owning pointer to new created Connection instance
   * @throws std::invalid_argument if ip or port invalid
   */
  std::shared_ptr<Remote::Connection>
  addConnection(const std::string &ip,
                std::uint_fast16_t port = IEC_60870_5_104_DEFAULT_PORT,
                ConnectionInit init = INIT_ALL);

  /**
   * @brief Get Connection object for a certain commonAddress
   * @param commonAddress common address that should be reached via remote
   * connection
   * @return Connection
   */
  std::shared_ptr<Remote::Connection>
  getConnectionFromCommonAddress(uint_fast16_t commonAddress);

  /**
   * @brief reestablish lost connections
   */
  void reconnectAll();

  /**
   * @brief close all connections to remote servers
   * @return information on operation success
   */
  void disconnectAll();

  /**
   * @brief set python callback that will be executed on incoming message from
   * unknown station
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnNewStationCallback(py::object &callable);

  void onNewStation(std::shared_ptr<Remote::Connection> connection,
                    std::uint_fast16_t common_address);

  /**
   * @brief set python callback that will be executed on incoming message from
   * unknown point
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnNewPointCallback(py::object &callable);

  void onNewPoint(std::shared_ptr<Object::Station> station,
                  std::uint_fast32_t io_address, IEC60870_5_TypeID type);

private:
  /**
   * @brief Create a new remote connection handler instance that acts as a
   * client
   * @details create a map of possible connections
   * @param tick_rate_ms intervall in milliseconds between the client checks connection states
   * @param timeout_ms timeout in milliseconds before an inactive connection
   * @param transport_security communication encryption instance reference
   * gets closed
   */
  Client(std::uint_fast32_t tick_rate_ms, std::uint_fast32_t timeout_ms,
         std::shared_ptr<Remote::TransportSecurity> transport_security);

  /// @brief tls handler
  std::shared_ptr<Remote::TransportSecurity> security{nullptr};

  /// @brief originator address of outgoing messages
  std::atomic_uint_fast8_t originatorAddress{0};

  /// @brief state that describes if the client component is enabled or not
  std::atomic_bool enabled{false};

  /// @brief timeout in milliseconds before an inactive connection gets closed
  std::atomic_uint_fast32_t commandTimeout_ms{1000};

  /// @brief MUTEX Lock to access connectionMap
  mutable Module::GilAwareMutex connections_mutex{"Client::connections_mutex"};

  /// @brief list of all created connections to remote servers
  Remote::ConnectionVector connections;

  /// @brief number of active connections
  std::atomic_uint_fast8_t activeConnections{0};

  /// @brief number of open connections
  std::atomic_uint_fast8_t openConnections{0};

  /// @brief minimum interval between to reconnects in milliseconds
  std::atomic_uint_fast32_t tickRate_ms{1000};

  /// @brief client thread to execute reconnects
  std::thread *runThread = nullptr;

  /// @brief client thread state
  std::atomic_bool running{false};

  /// @brief client thread mutex to not lock thread execution
  mutable std::mutex runThread_mutex{};

  /// @brief conditional variable to stop client thread without waiting for
  /// another loop
  mutable std::condition_variable runThread_wait{};

  /// @brief python callback function pointer
  Module::Callback<void> py_onNewStation{
      "Client.on_new_station", "(client: c104.Client, connection: "
                               "c104.Connection, common_address: int) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onNewPoint{
      "Client.on_new_point", "(client: c104.Client, station: c104.Station, "
                             "io_address: int, point_type: c104.Type) -> None"};

  /// @brief client thread function
  void thread_run();

  /**
   * @brief Get Connection object for a certain connection identified via
   * connectionString
   * @param connectionString ip:port string of remote server
   * @return Connection
   */
  std::shared_ptr<Remote::Connection>
  getConnectionFromString(const std::string &connectionString);
};

#endif // C104_CLIENT_H
