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
 * @file Connection.h
 * @brief manage 60870-5-104 connection from scada to a remote terminal unit
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_CONNECTION_H
#define C104_REMOTE_CONNECTION_H

#include "module/Callback.h"
#include "module/GilAwareMutex.h"
#include "object/Station.h"
#include "types.h"

namespace Remote {
/**
 * @brief connection model for connections via client component to remote
 * servers
 */
class Connection : public std::enable_shared_from_this<Connection> {
public:
  // noncopyable
  Connection(const Connection &) = delete;
  Connection &operator=(const Connection &) = delete;

  /**
   * @brief Create a new (still closed) connection to a remote server identified
   * via ip and port
   * @param client client instance reference
   * @param ip ip address or hostname of remote server
   * @param port port address of remote server
   * @param command_timeout_ms default timeout for command confirmation
   * @param init connection initialization procedure
   * @param transport_security communication encryption instance reference
   * @param originator_address client identification address
   * @return owning pointer of new Connection instance
   * @throws std::invalid_argument if ip or port invalid
   */
  [[nodiscard]] static std::shared_ptr<Connection> create(
      std::shared_ptr<Client> client, const std::string &ip,
      const uint_fast16_t port = IEC_60870_5_104_DEFAULT_PORT,
      const uint_fast32_t command_timeout_ms = 4000,
      const ConnectionInit init = INIT_ALL,
      std::shared_ptr<Remote::TransportSecurity> transport_security = nullptr,
      const uint_fast8_t originator_address = 0) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Connection>(
        new Connection(std::move(client), ip, port, command_timeout_ms, init,
                       std::move(transport_security), originator_address));
  }

  /**
   * @brief Close and destroy a connection to a remote server
   */
  ~Connection();

  /**
   * Getter for connectionString to remote server
   * @return ip:port string
   */
  std::string getConnectionString() const;

  /**
   * Getter for ip of remote server
   * @return ip string
   */
  std::string getIP() const;

  /**
   * Getter for port of remote server
   * @return port number
   */
  std::uint_fast16_t getPort() const;

  /**
   * Getter for connection state
   * @return connection state enum
   */
  ConnectionState getState() const;

  /**
   * @brief Setter for originatorAddress: who is the originator of a client
   * message
   * @param address originator address of a client message
   */
  void setOriginatorAddress(std::uint_fast8_t address);

  /**
   * @brief Getter for originatorAddress: who is the originator of a client
   * message
   * @return originator address of a client message
   */
  std::uint_fast8_t getOriginatorAddress() const;

  std::shared_ptr<Client> getClient() const;

  // Station accessors

  /**
   * @brief Open a created connection to remote server
   */
  void connect();

  /**
   * @brief Close a created connection to remote server
   * @return information on operation success
   */
  void disconnect();

  /**
   * @brief Test if connection to remote server is open
   * @return information on open state
   */
  bool isOpen() const;

  /**
   * @brief Test if connection to remote server is muted
   * @return information on muted state
   */
  bool isMuted() const;

  /**
   * @brief Mute an open connection to remote server - disable messages from
   * server to client
   * @return information on operation success
   */
  bool mute();

  /**
   * @brief Unmute an open connection to remote server - enabled messages from
   * server to client
   * @return information on operation success
   */
  bool unmute();

  // Client accessors

  /**
   * @brief Setter for muted state
   * @param value value of new muted state (true = muted, false = unmuted)
   */
  bool setMuted(bool value);

  /**
   * @brief Getter for internal connection object
   * @return CS104_Connection refrence
   */
  CS104_Connection getCS104();

  /**
   * @brief Setter for open state: Mark connection as open
   */
  bool setOpen();

  /**
   * @brief Setter for open state: Mark connection as closed, start reconnect
   * state
   */
  bool setClosed();

  /**
   * @brief add command id to awaiting command result map
   * @param cmdId unique command id
   * @param state command process state
   * @returns if command preparation was successfully (no collision with active
   * sequence)
   */
  bool prepareCommandSuccess(const std::string &cmdId,
                             CommandProcessState state);

  /**
   * @brief mark a command success as failed to fail fast
   * @param cmdId unique command id
   */
  void cancelCommandSuccess(const std::string &cmdId);

  /**
   * @brief Wait for command confirmation and success information, release
   * outgoing message LOCK for this command and get information on last commands
   * success
   * @param cmdId unique command id
   * @return information on last command success
   */
  bool awaitCommandSuccess(const std::string &cmdId);

  /**
   * @brief Set success state of last command
   * @param message incoming message
   */
  void setCommandSuccess(std::shared_ptr<Message::IncomingMessage> message);

  /**
   * @brief Test if Stations exists at this NetworkStation
   * @return information on availability of child Station objects
   */
  bool hasStations() const;

  /**
   * @brief Get a list of all Stations
   * @return vector with object stationer
   */
  Object::StationVector getStations() const;

  /**
   * @brief Get a Station that exists at this NetworkStation and is identified
   * via information object address
   * @return Stationer to Station or nullptr
   */
  std::shared_ptr<Object::Station>
  getStation(std::uint_fast16_t commonAddress) const;

  /**
   * @brief Test if Stations exists at this NetworkStation
   * @return information on availability of child Station objects
   */
  bool hasStation(std::uint_fast16_t commonAddress) const;

  /**
   * @brief Add a Station to this Station
   */
  std::shared_ptr<Object::Station> addStation(std::uint_fast16_t commonAddress);

  /**
   * @brief set python callback that will be executed on incoming message
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnReceiveRawCallback(py::object &callable);

  void onReceiveRaw(unsigned char *msg, unsigned char msgSize);

  /**
   * @brief set python callback that will be executed on outgoing message
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnSendRawCallback(py::object &callable);

  void onSendRaw(unsigned char *msg, unsigned char msgSize);

  /**
   * @brief set python callback that will be executed on connection state
   * changes
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnStateChangeCallback(py::object &callable);

  /**
   * @brief send interrogation command
   * @param commonAddress
   * @param cause
   * @param qualifier
   * @param wait_for_response
   * @return success information
   * @throws std::invalid_argument if qualifier is invalid
   */
  bool interrogation(std::uint_fast16_t commonAddress,
                     CS101_CauseOfTransmission cause = CS101_COT_ACTIVATION,
                     CS101_QualifierOfInterrogation qualifier = QOI_STATION,
                     bool wait_for_response = true);

  /**
   * @brief send counter interrogation command
   * @param commonAddress station address
   * @param cause transmission reason
   * @param qualifier parameter for counter interrogation
   * @param wait_for_response blocking or non-blocking
   * @return success information
   * @throws std::invalid_argument if qualifier is invalid
   */
  bool
  counterInterrogation(std::uint_fast16_t commonAddress,
                       CS101_CauseOfTransmission cause = CS101_COT_ACTIVATION,
                       QualifierOfCIC qualifier = IEC60870_QCC_RQT_GENERAL,
                       bool wait_for_response = true);

  /**
   * @brief send clock synchronization command
   * @param commonAddress station address
   * @param wait_for_response blocking or non-blocking
   * @return success information
   */
  bool clockSync(std::uint_fast16_t commonAddress,
                 bool wait_for_response = true);

  /**
   * @brief send test command
   * @param commonAddress station address
   * @param with_time include a timestamp in the test command
   * @param wait_for_response blocking or non-blocking
   * @return success information
   */
  bool test(std::uint_fast16_t commonAddress, bool with_time = true,
            bool wait_for_response = true);

  /**
   * @brief transmit a command to a remote server
   * @param point control point
   * @param cause reason for transmission
   * @returns if operation was successful
   * @throws std::invalid_argument if point type is not supported for this operation
   */
  bool transmit(std::shared_ptr<Object::DataPoint> point,
                CS101_CauseOfTransmission cause);

  /**
   * @brief add command id to awaiting command result map
   * @param message outgoing message
   * @param wait_for_response blocking or non-blocking
   * @param state command process state
   * @returns if command preparation was successfully (no collision with active sequence)
   */
  bool command(std::shared_ptr<Message::OutgoingMessage> message,
               bool wait_for_response = true,
               CommandProcessState state = COMMAND_AWAIT_CON);

  /**
   * @brief send a point read command to remote server
   * @param point monitoring point
   * @param wait_for_response blocking or non-blocking
   * @returns if operation was successful
   * @throws std::invalid_argument if point type is not supported for this operation
   */
  bool read(std::shared_ptr<Object::DataPoint> point,
            bool wait_for_response = true);

  /**
   * @brief Callback for logging incoming and outgoing byteStreams
   * @param parameter reference to custom bound connection data
   * @param msg pointer to first character of message
   * @param msgSize character count of message
   * @param sent direction of message
   * @warning DEBUG FUNCTION, IN ORDER TO ACTIVE THIS CALLBACK REMOVE COMMENT
   * WRAPPER IN Connect(connectionState)
   */
  static void rawMessageHandler(void *parameter, uint_fast8_t *msg, int msgSize,
                                bool sent);

  /**
   * @brief Callback to handle connection state changes
   * @param parameter reference to custom bound connection data
   * @param connection internal CS104_Connection connection object reference
   * @param event state change event (opened,closed,muted,unmuted identified via constants)
   */
  static void connectionHandler(void *parameter, CS104_Connection connection,
                                CS104_ConnectionEvent event);

  /**
   * @brief Callback to handle incomming reports from remote servers
   * @param parameter reference to custom bound connection data
   * @param address NOT USED IN CS104 - relict from CS101
   * @param asdu incoming message formatted as ASDU object
   * @return
   */
  static bool asduHandler(void *parameter, int address, CS101_ASDU asdu);

private:
  /**
   * @brief Create a new (still closed) connection to a remote server identified
   * via ip and port
   * @param _client client instance reference
   * @param _ip ip address or hostname of remote server
   * @param _port port address of remote server
   * @param command_timeout_ms default timeout for command confirmation
   * @param init connection initialization procedure
   * @param transport_security communication encryption instance reference
   * @param originator_address client identification address
   * @throws std::invalid_argument if ip or port invalid
   */
  Connection(std::shared_ptr<Client> _client, const std::string &_ip,
             uint_fast16_t _port, uint_fast32_t command_timeout_ms,
             ConnectionInit init,
             std::shared_ptr<Remote::TransportSecurity> transport_security,
             uint_fast8_t originator_address);

  /// @brief client object reference
  std::weak_ptr<Client> client{};

  /// @brief MUTEX Lock to access non atomic connection information
  mutable Module::GilAwareMutex connection_mutex{
      "Connection::connection_mutex"};

  /// @brief timeout in milliseconds before an inactive connection gets closed
  std::uint_fast32_t commandTimeout_ms{1000};

  /// @brief IP address of remote server
  std::string ip = "";

  /// @brief Port of remote server
  std::uint_fast16_t port = 0;

  /// @brief connection initialization commands
  std::atomic<ConnectionInit> init{INIT_ALL};

  /// @brief originator address of outgoing messages
  std::atomic_uint_fast8_t originatorAddress{0};

  /// @brief connectionString to remote server (ip:port)
  std::string connectionString = "";

  /// @brief internal connection object
  CS104_Connection connection = nullptr;

  /// @brief how often was a connection opened successfully to remote server
  std::atomic_uint_fast16_t connectionCount{0};

  /// @brief current state of state machine behaviour
  std::atomic<ConnectionState> state{CLOSED};

  /// @brief timestamp of last successfully connection opening
  std::atomic_uint_fast64_t connectedAt_ms{0};

  /// @brief timestamp of last disconnect
  std::atomic_uint_fast64_t disconnectedAt_ms{0};

  /// @brief MUTEX Lock to wait for command response
  mutable Module::GilAwareMutex expectedResponseMap_mutex{
      "Connection::expectedResponseMap_mutex"};

  /// @brief awaited command responses (must be access with
  /// expectedResponseMap_mutex)
  std::map<std::string, CommandProcessState> expectedResponseMap{};

  /// @brief currently active command sequence, if any (must be access with
  /// expectedResponseMap_mutex)
  std::string sequenceId{""};

  /// @brief Condition to wait for successfully command confirmation and success
  /// information or timeout
  std::condition_variable_any response_wait{};

  /// @brief vector of stations accessible via this connection
  Object::StationVector stations{};

  /// @brief access mutex to lock station vector access
  mutable Module::GilAwareMutex stations_mutex{"Connection::stations_mutex"};

  /// @brief sequence counter number
  std::atomic_uint_fast64_t testSequenceCounter{0};

  /// @brief python callback function pointer
  Module::Callback<void> py_onReceiveRaw{
      "Connection.on_receive_raw",
      "(connection: c104.Connection, data: bytes) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onSendRaw{
      "Connection.on_send_raw",
      "(connection: c104.Connection, data: bytes) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onStateChange{
      "Connection.on_state_change",
      "(connection: c104.Connection, state: c104.ConnectionState) -> None"};

  /**
   * Getter for connection state
   * @return connection state enum
   */
  void setState(ConnectionState connectionState);
};

/**
 * @brief vector definition of Connection objects
 */
typedef std::vector<std::shared_ptr<Connection>> ConnectionVector;

} // namespace Remote

#endif // C104_REMOTE_CONNECTION_H
