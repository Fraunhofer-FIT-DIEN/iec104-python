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
      const uint_fast16_t command_timeout_ms = 10000,
      const ConnectionInit init = INIT_ALL,
      std::shared_ptr<Remote::TransportSecurity> transport_security = nullptr,
      const uint_fast8_t originator_address = 0) {
    // Not using std::make_shared because the constructor is private.
    auto connection = std::shared_ptr<Connection>(
        new Connection(std::move(client), ip, port, command_timeout_ms, init,
                       std::move(transport_security), originator_address));

    // track reference as weak pointer for safe static callbacks
    void *key =
        static_cast<void *>(connection.get()); // Use `this` as a unique key
    std::lock_guard<std::mutex> lock(instanceMapMutex);
    instanceMap[key] = connection;

    return connection;
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

  /**
   * @brief getter for client
   * @return shared pointer to the owning client instance, optional
   */
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
  void setMuted(bool value);

  /**
   * @brief Setter for open state: Mark connection as open
   */
  void setOpen();

  /**
   * @brief Setter for open state: Mark connection as closed, start reconnect
   * state
   */
  void setClosed();

  /**
   * @brief add command id to awaiting command result map
   * @param cmdId unique command id
   * @param state command process state
   * @throws std::runtime_error if cmdId already in use
   */
  void prepareCommandSuccess(const std::string &cmdId,
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
   * @brief Checks whether a remote Station with the given common address exists
   * on this client Connection.
   * @param commonAddress The common address (CA) used to identify the Station.
   * @return True if a Station with the specified common address exists,
   * otherwise false.
   */
  bool hasStation(std::uint_fast16_t commonAddress) const;

  /**
   * @brief Adds a new remote Station to this client Connection.
   * @param commonAddress The common address (CA) that uniquely identifies the
   * new Station.
   * @return A shared pointer to the newly added Station.
   */
  std::shared_ptr<Object::Station> addStation(std::uint_fast16_t commonAddress);

  /**
   * @brief Removes an existing remote Station from this client Connection.
   *
   * @param commonAddress The common address (CA) of the Station to be removed.
   * @return True if the Station was successfully removed, otherwise false.
   */
  bool removeStation(std::uint_fast16_t commonAddress);

  /**
   * @brief Get a reference to the protocol parameters to be able to read and
   * update these
   */
  CS104_APCIParameters getParameters() const;

  /**
   * @brief set python callback that will be executed on incoming message
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnReceiveRawCallback(py::object &callable);

  /**
   * @brief Execute configured callback handlers on receiving raw messages.
   *
   * @param msg Pointer to the raw message data.
   * @param msgSize Size of the raw message data in bytes.
   */
  void onReceiveRaw(unsigned char *msg, unsigned char msgSize);

  /**
   * @brief set python callback that will be executed on outgoing message
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnSendRawCallback(py::object &callable);

  /**
   * @brief Execute configured callback handlers on sending raw messages.
   *
   * @param msg Pointer to the raw message data to be sent.
   * @param msgSize Size of the raw message in bytes.
   */
  void onSendRaw(unsigned char *msg, unsigned char msgSize);

  /**
   * @brief set python callback that will be executed on connection state
   * changes
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnStateChangeCallback(py::object &callable);

  /**
   * @brief set python callback that will be executed on unexpected incoming
   * messages
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnUnexpectedMessageCallback(py::object &callable);

  /**
   * @brief Execute configured callback handlers on receiving unexpected
   * messages from a client
   *
   * @param connection The master connection from which the unexpected message
   * was received.
   * @param message A shared pointer to the incoming message that triggered the
   * unexpected event.
   * @param cause The reason why the message was classified as unexpected (e.g.,
   * unknown type ID or invalid cause-of-transmission).
   */
  void onUnexpectedMessage(std::shared_ptr<Message::IncomingMessage> message,
                           UnexpectedMessageCause cause);

  /**
   * @brief getter for connectedAt
   * @return the time point the currently active connection was established,
   * optional
   */
  std::optional<std::chrono::system_clock::time_point> getConnectedAt() const;

  /**
   * @brief getter for disconnectedAt
   * @return the time point the last connection was disconnected, if currently
   * not connected, optional
   */
  std::optional<std::chrono::system_clock::time_point>
  getDisconnectedAt() const;

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
   * @param qualifier targeted counters
   * @param freeze counter behaviour
   * @param wait_for_response blocking or non-blocking
   * @return success information
   * @throws std::invalid_argument if qualifier is invalid
   */
  bool
  counterInterrogation(std::uint_fast16_t commonAddress,
                       CS101_CauseOfTransmission cause = CS101_COT_ACTIVATION,
                       CS101_QualifierOfCounterInterrogation qualifier =
                           CS101_QualifierOfCounterInterrogation::GENERAL,
                       CS101_FreezeOfCounterInterrogation freeze =
                           CS101_FreezeOfCounterInterrogation::READ,
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
   * @throws std::invalid_argument if point type is not supported for this
   * operation
   */
  bool transmit(std::shared_ptr<Object::DataPoint> point,
                CS101_CauseOfTransmission cause);

  /**
   * @brief add command id to awaiting command result map
   * @param message outgoing message
   * @param wait_for_response blocking or non-blocking
   * @param state command process state
   * @returns if command preparation was successfully (no collision with active
   * sequence)
   */
  bool command(std::shared_ptr<Message::OutgoingMessage> message,
               bool wait_for_response = true,
               CommandProcessState state = COMMAND_AWAIT_CON);

  /**
   * @brief send a point read command to remote server
   * @param point monitoring point
   * @param wait_for_response blocking or non-blocking
   * @returns if operation was successful
   * @throws std::invalid_argument if point type is not supported for this
   * operation
   */
  bool read(std::shared_ptr<Object::DataPoint> point,
            bool wait_for_response = true);

  /**
   * Retrieves the shared instance of the Connection associated with the given
   * key.
   *
   * This function is a thread-safe method to access Connection instances stored
   * in an internal instance map. A weak reference is used for storage, and the
   * function returns a shared pointer to the associated Connection instance. If
   * the key is not found in the map or the associated weak reference has
   * expired, a null shared pointer is returned.
   *
   * @param key A pointer serving as the unique identifier for the desired
   * Connection instance.
   * @return A shared pointer to the Connection instance associated with the
   * given key, or a null shared pointer if the key is not found or the instance
   * has been deallocated.
   */
  static std::shared_ptr<Connection> getInstance(void *key);

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
   * @param event state change event (opened,closed,muted,unmuted identified via
   * constants)
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
             uint_fast16_t _port, uint_fast16_t command_timeout_ms,
             ConnectionInit init,
             std::shared_ptr<Remote::TransportSecurity> transport_security,
             uint_fast8_t originator_address);

  /// @brief client object reference
  std::weak_ptr<Client> client{};

  /// @brief MUTEX Lock to access non atomic connection information
  mutable Module::GilAwareMutex connection_mutex{
      "Connection::connection_mutex"};

  /// @brief timeout in milliseconds before an inactive connection gets closed
  std::atomic_uint_fast16_t commandTimeout_ms{10000};

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
  std::atomic<std::chrono::system_clock::time_point> connectedAt{};

  /// @brief timestamp of last disconnect
  std::atomic<std::chrono::system_clock::time_point> disconnectedAt{};

  /// @brief MUTEX Lock to wait for command response
  mutable Module::GilAwareMutex expectedResponseMap_mutex{
      "Connection::expectedResponseMap_mutex"};

  /// @brief awaited command responses (must be access with
  /// expectedResponseMap_mutex)
  std::map<std::string, CommandProcessState> expectedResponseMap{};

  /// @brief count expected occurrences of command responses (must be access
  /// with expectedResponseMap_mutex)
  std::map<std::string, std::uint_fast16_t> expectedResponseMapCount{};

  /// @brief Condition to wait for successfully command confirmation and success
  /// information or timeout
  std::condition_variable_any response_wait{};

  /// @brief vector of stations accessible via this connection
  Object::StationVector stations{};

  /// @brief access mutex to lock station vector access
  mutable Module::GilAwareMutex stations_mutex{"Connection::stations_mutex"};

  /// @brief sequence counter number
  std::atomic_uint_fast64_t testSequenceCounter{0};

  /// @brief instance weak pointer list for safe static callbacks
  static std::unordered_map<void *, std::weak_ptr<Connection>> instanceMap;

  /// @brief mutex to lock instanceMap read/write access
  static std::mutex instanceMapMutex;

  /// @brief python callback function pointer
  Module::Callback<void> py_onReceiveRaw{
      "Connection.on_receive_raw",
      "(connection: c104.Connection, data: bytes) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onSendRaw{
      "Connection.on_send_raw",
      "(connection: c104.Connection, data: bytes) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onUnexpectedMessage{
      "Connection.on_unexpected_message",
      "(connection: c104.Connection, message: c104.IncomingMessage, cause: "
      "c104.Umc) "
      "-> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onStateChange{
      "Connection.on_state_change",
      "(connection: c104.Connection, state: c104.ConnectionState) -> None"};

  /**
   * @brief update the connection state and trigger the on state change callback
   * handler
   */
  void setState(ConnectionState connectionState);

public:
  /**
   * @brief Returns a string representation of the Connection object, including
   * its state, IP address, port, number of stations, and memory address.
   *
   * @return A formatted string describing the current state of the Connection.
   */
  std::string toString() const {
    size_t len = 0;
    {
      std::scoped_lock<Module::GilAwareMutex> const lock(stations_mutex);
      len = stations.size();
    }
    std::ostringstream oss;
    oss << "<104.Connection ip=" << ip << ", port=" << std::to_string(port)
        << ", state=" << ConnectionState_toString(state)
        << ", #stations=" << std::to_string(len) << " at " << std::hex
        << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  };
};

/**
 * @brief vector definition of Connection objects
 */
typedef std::vector<std::shared_ptr<Connection>> ConnectionVector;

} // namespace Remote

#endif // C104_REMOTE_CONNECTION_H
