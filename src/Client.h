/**
 * Copyright 2020-2025 Fraunhofer Institute for Applied Information Technology
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

  /**
   * @brief Factory method to create a shared pointer to a Client instance.
   *
   * @param tick_rate_ms Tick rate in milliseconds for the Client execution
   * loop. Defaults to 100.
   * @param timeout_ms Timeout in milliseconds for Client operations. Defaults
   * to 100.
   * @param transport_security Shared pointer to a TransportSecurity object for
   * remote communication. Defaults to nullptr.
   * @return A shared pointer to the newly created Client instance.
   */
  [[nodiscard]] static std::shared_ptr<Client> create(
      std::uint_fast16_t tick_rate_ms = 100,
      std::uint_fast16_t timeout_ms = 100,
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

  /**
   * @brief Checks if the client has active (established and not muted)
   * connections.
   * @return True if there are active connections, otherwise false.
   */
  bool hasConnections();

  /**
   * @brief Test if Client has open connections to clients
   * @return information if at least one connection exists
   */
  bool hasOpenConnections() const;

  /**
   * @brief get number of open connections to servers
   * @return open connection count
   */
  std::uint_fast8_t getOpenConnectionCount() const;

  /**
   * @brief Test if Client has active (open and not muted) connections to
   * servers
   * @return information if at least one connection is active
   */
  bool hasActiveConnections() const;

  /**
   * @brief get number of active (open and not muted) connections to servers
   * @return active connection count
   */
  std::uint_fast8_t getActiveConnectionCount() const;

  /**
   * @brief Retrieves the current list of active connections established by the
   * client.
   *
   * This method provides access to the client's connection vector, which
   * contains shared pointers to all active connections established with remote
   * servers.
   *
   * @return A vector of shared pointers to Connection objects representing
   * active connections.
   */
  Remote::ConnectionVector getConnections();

  /**
   * @brief Checks if a connection exists for the specified IP and port.
   *
   * @param ip The IP address to check for a connection.
   * @param port The port number to check for a connection.
   * @return True if a connection exists, false otherwise.
   */
  bool hasConnection(const std::string &ip,
                     std::uint_fast16_t port = IEC_60870_5_104_DEFAULT_PORT);

  /**
   * @brief Retrieves a connection to a remote host using the specified IP
   * address and port.
   *
   * @param ip The IP address of the remote host.
   * @param port The port number to connect to on the remote host.
   * @return A shared pointer to the established connection.
   */
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

  /**
   * @brief Execute configured callback handlers on receiving station
   * information from a not yet known common address.
   *
   * @param connection A shared pointer to the connection object representing
   * the remote connection.
   * @param common_address The common address associated with the new station.
   */
  void onNewStation(std::shared_ptr<Remote::Connection> connection,
                    std::uint_fast16_t common_address);

  /**
   * @brief set python callback that will be executed on incoming message from
   * unknown point
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnNewPointCallback(py::object &callable);

  /**
   * @brief Execute configured callback handlers on receiving point information
   * from a not yet known information object address.
   *
   * @param station Shared pointer to the station object where the point is to
   * be added.
   * @param io_address The input/output address associated with the new point.
   * @param type The type identifier for the new point, conforming to the
   * IEC60870-5 standard.
   */
  void onNewPoint(std::shared_ptr<Object::Station> station,
                  std::uint_fast32_t io_address, IEC60870_5_TypeID type);

  /**
   * @brief set python callback that will be executed on incoming end of
   * initialization messages
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnEndOfInitializationCallback(py::object &callable);

  /**
   * @brief Execute configured callback handlers on receiving and end of
   * initialization message from a station.
   *
   * @param station A shared pointer to the station object representing the
   * initialized station.
   * @param cause The cause of initialization, indicating the reason for the
   * initialization event.
   */
  void onEndOfInitialization(std::shared_ptr<Object::Station> station,
                             CS101_CauseOfInitialization cause);

  /**
   * @brief getter for tickRate_ms
   * @return minimum interval between two periodic tasks
   */
  std::uint_fast16_t getTickRate_ms() const;

  /**
   * @brief Schedules a periodic task to be executed at a specified interval.
   *
   * @param task A callable object representing the task to be executed
   * periodically.
   * @param interval The interval in milliseconds at which the task should be
   * executed. Must be at least 1000ms. Throws std::out_of_range if the interval
   * is less than 50ms.
   */
  void schedulePeriodicTask(const std::function<void()> &task, int interval);

  /**
   * @brief Schedules a task to be executed after a specified delay (or
   * instant).
   *
   * The order of execution will depend on the timestamp calculated from current
   * time + delay. The delay may be negative for high priority tasks.
   *
   * @param task The function to be executed.
   * @param delay The delay in milliseconds before the task is executed. A
   * negative delay executes the task immediately.
   */
  void scheduleTask(const std::function<void()> &task, int delay = 0);

private:
  /**
   * @brief Create a new remote connection handler instance that acts as a
   * client
   * @details create a map of possible connections
   * @param tick_rate_ms intervall in milliseconds between the client checks
   * connection states
   * @param timeout_ms timeout in milliseconds before an inactive connection
   * @param transport_security communication encryption instance reference
   * gets closed
   */
  Client(std::uint_fast16_t tick_rate_ms, std::uint_fast16_t timeout_ms,
         std::shared_ptr<Remote::TransportSecurity> transport_security);

  /**
   * @brief Schedules timers for data points associated with clients' stations.
   *
   * This method iterates through all active client connections and their
   * respective stations. For each data point that has a timer set to execute
   * before the current time, it schedules a task to trigger the data point's
   * timer callback.
   */
  void scheduleDataPointTimer();

  /// @brief minimum interval between two periodic tasks
  const std::uint_fast16_t tickRate_ms{1000};

  /// @brief timeout in milliseconds before an inactive connection gets closed
  const std::uint_fast16_t commandTimeout_ms{100};

  /// @brief tls handler
  const std::shared_ptr<Remote::TransportSecurity> security{nullptr};

  /// @brief originator address of outgoing messages
  std::atomic_uint_fast8_t originatorAddress{0};

  /// @brief state that describes if the client component is enabled or not
  std::atomic_bool enabled{false};

  /// @brief client thread state
  std::atomic_bool running{false};

  /// @brief MUTEX Lock to access connectionMap
  mutable Module::GilAwareMutex connections_mutex{"Client::connections_mutex"};

  /// @brief list of all created connections to remote servers
  Remote::ConnectionVector connections;

  std::priority_queue<Task> tasks;

  /// @brief client thread to execute reconnects
  std::thread *runThread = nullptr;

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

  /// @brief python callback function pointer
  Module::Callback<void> py_onEndOfInitialization{
      "Client.on_station_initialized",
      "(client: c104.Client, station: c104.Station, "
      "cause: c104.Coi) -> None"};

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

public:
  /**
   * @brief Converts the current client instance state into a string
   * representation.
   *
   * @return A string representation of the client instance, including
   * originator address, number of active connections, and memory address of the
   * object.
   */
  std::string toString() const {
    size_t len = 0;
    {
      std::scoped_lock<Module::GilAwareMutex> const lock(connections_mutex);
      len = connections.size();
    }
    std::ostringstream oss;
    oss << "<104.Client originator_address="
        << std::to_string(originatorAddress.load())
        << ", #connections=" << std::to_string(len) << " at " << std::hex
        << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  };
};

#endif // C104_CLIENT_H
