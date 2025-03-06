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
 * @file Server.h
 * @brief operate a remote terminal unit
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_SERVER_H
#define C104_SERVER_H

#include "remote/Helper.h"
#include "types.h"

#include "module/Callback.h"
#include "module/GilAwareMutex.h"
#include "object/Station.h"
#include "remote/TransportSecurity.h"
#include "remote/message/IncomingMessage.h"

/**
 * @brief Represents a selection within the server for select-and-execute
 * patterns.
 *
 * The `Selection` structure is used to maintain information related to a
 * specific select-and-execute process. This includes details such as the select
 * ASDU, originating address (OA), common address (CA), information object
 * address (IOA), and the master connection that initiated the selection.
 * Additionally, it stores the timestamp indicating when the selection was
 * created to test for timeouts.
 */
struct Selection {
  CS101_ASDU asdu;
  uint8_t oa;
  uint16_t ca;
  uint32_t ioa;
  IMasterConnection connection;
  std::chrono::steady_clock::time_point created;
};

/**
 * @brief service model for IEC60870-5-104 communication as server
 */
class Server : public std::enable_shared_from_this<Server> {

  // @todo import/export station with DataPoints to string
public:
  // noncopyable
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  /**
   * @brief Factory method to create an instance of the Server class.
   *
   * Provides a convenient way to instantiate a `Server` object using
   * shared pointers. The constructor of `Server` is private, so this
   * static method ensures proper allocation and initialization. The method
   * allows customization of the bind IP address, TCP port, tick rate,
   * selection timeout, the maximum number of open connections, and
   * transport security.
   *
   * @param bind_ip The IP address on which the server will accept connections.
   * Defaults to "0.0.0.0".
   * @param tcp_port The TCP port for the server. Defaults to
   * IEC_60870_5_104_DEFAULT_PORT.
   * @param tick_rate_ms The interval, in milliseconds, at which the server
   * executes periodic tasks. Defaults to 100 ms.
   * @param select_timeout_ms The timeout, in milliseconds, for select
   * operations. Defaults to 100 ms.
   * @param max_open_connections The maximum number of simultaneous open
   * connections allowed. A value of 0 means no limit. Defaults to 0.
   * @param transport_security A shared pointer to a TransportSecurity object to
   * enable secure communication. Defaults to nullptr.
   * @return A `std::shared_ptr` to the created `Server` instance.
   */
  [[nodiscard]] static std::shared_ptr<Server> create(
      std::string bind_ip = "0.0.0.0",
      uint_fast16_t tcp_port = IEC_60870_5_104_DEFAULT_PORT,
      uint_fast16_t tick_rate_ms = 100, uint_fast16_t select_timeout_ms = 100,
      std::uint_fast8_t max_open_connections = 0,
      std::shared_ptr<Remote::TransportSecurity> transport_security = nullptr) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Server>(
        new Server(bind_ip, tcp_port, tick_rate_ms, select_timeout_ms,
                   max_open_connections, std::move(transport_security)));
  }

  // DESTRUCTOR

  /**
   * @brief Close and destroy server
   */
  ~Server();

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
   * @brief Sets the maximum number of open connections the server can handle
   * simultaneously.
   *
   * This method updates the internal limit for the maximum number of concurrent
   * open connections allowed on the server. When the limit changes, it updates
   * the relevant subsystem responsible for enforcing this constraint.
   *
   * @param max_open_connections The new maximum number of open connections the
   * server should allow.
   */
  void setMaxOpenConnections(std::uint_fast8_t max_open_connections);

  /**
   * @brief Retrieves the maximum number of connections that can be
   * simultaneously open on the server.
   *
   * This method returns the predetermined upper limit for concurrent open
   * connections allowed by the server. It helps manage server capacity and
   * ensures that system resources are properly allocated and protected from
   * exceeding this limit.
   *
   * @return The maximum number of open connections allowed on the server.
   */
  std::uint_fast8_t getMaxOpenConnections() const;

  // CONNECTION HANDLING

  /**
   * @brief start listening for client connections, send periodic broadcasts
   * @throws std::runtime_error if slave thread failed to start
   */
  void start();

  /**
   * @brief stop listening for client connections
   */
  void stop();

  /**
   * @brief test if server is currently active
   * @return information on active state of server
   */
  bool isRunning();

  /**
   * @brief Test if Stations exists at this NetworkStation
   * @return information on availability of child Station objects
   */
  bool hasStations() const;

  /**
   * @brief Checks if a given master connection already exists in the server.
   *
   * This method inspects the internal connection map to determine whether
   * the provided master connection has already been registered with the server.
   *
   * @param connection The master connection to verify its existence.
   * @return True if the connection exists, otherwise false.
   */
  bool isExistingConnection(IMasterConnection connection);

  /**
   * @brief Test if Server has open connections to clients
   * @return information if at least one connection exists
   */
  bool hasOpenConnections() const;

  /**
   * @brief get number of open connections to clients
   * @return open connection count
   */
  std::uint_fast8_t getOpenConnectionCount() const;

  /**
   * @brief Test if Server has active (open and not muted) connections to
   * clients
   * @return information if at least one connection is active
   */
  bool hasActiveConnections() const;

  /**
   * @brief get number of active (open and not muted) connections to clients
   * @return active connection count
   */
  std::uint_fast8_t getActiveConnectionCount() const;

  /**
   * @brief Get a list of all Stations
   * @return vector with station objects
   */
  Object::StationVector getStations() const;

  /**
   * @brief Retrieves a Station that exists on this Server, identified by the
   * given information object address.
   * @param ca The common address (CA) that uniquely identifies the Station.
   * @return A shared pointer to the Station if it exists, or nullptr if no
   * matching Station is found.
   */
  std::shared_ptr<Object::Station>
  getStation(std::uint_fast16_t commonAddress) const;

  /**
   * @brief Checks whether a Station with the given common address exists on
   * this Server.
   * @param commonAddress The common address (CA) used to identify the Station.
   * @return True if a Station with the specified common address exists,
   * otherwise false.
   */
  bool hasStation(std::uint_fast16_t commonAddress) const;

  /**
   * @brief Adds a new Station to this Server.
   * @param commonAddress The common address (CA) that uniquely identifies the
   * new Station.
   * @return A shared pointer to the newly added Station.
   */
  std::shared_ptr<Object::Station> addStation(std::uint_fast16_t commonAddress);

  /**
   * @brief Removes an existing Station from this Server.
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
   * @param msg Pointer to the raw message data received.
   * @param msgSize Size of the raw message data received in bytes.
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
   * @param msg Pointer to the raw message data received.
   * @param msgSize Size of the raw message data received in bytes.
   */
  void onSendRaw(unsigned char *msg, unsigned char msgSize);

  /**
   * @brief set python callback that will be executed on incoming clock sync
   * command
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnClockSyncCallback(py::object &callable);

  /**
   * @brief Handles the clock synchronization request initiated from a specific
   * client.
   *
   * This method processes a clock synchronization request from a client,
   * encapsulated with the client's IP address and the target synchronization
   * time. It facilitates actions such as invoking a Python callback, processing
   * the time data into a structured format, and determining the appropriate
   * response state.
   *
   * @param _ip The IP address of the client requesting clock synchronization.
   * @param time The target synchronization time represented as a
   * std::chrono::system_clock::time_point object.
   * @return CommandResponseState indicating the result of the synchronization
   * process. Possible values include:
   *         - RESPONSE_STATE_SUCCESS: The synchronization process was
   * successful.
   *         - RESPONSE_STATE_FAILURE: The synchronization process failed.
   *         - RESPONSE_STATE_NONE: No specific response was generated.
   */
  CommandResponseState onClockSync(std::string _ip,
                                   std::chrono::system_clock::time_point time);

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
  void
  onUnexpectedMessage(IMasterConnection connection,
                      std::shared_ptr<Remote::Message::IncomingMessage> message,
                      UnexpectedMessageCause cause);

  /**
   * @brief set python callback that will be executed on incoming connection
   * requests
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnConnectCallback(py::object &callable);

  /**
   * @brief getter for tickRate_ms
   * @return minimum interval between two periodic tasks
   */
  std::uint_fast16_t getTickRate_ms() const;

  /**
   * @brief transmit a datapoint related message to a remote client
   * @param point datapoint that should be send via server
   * @param cause reason for transmission
   * @return information on operation success
   * @throws std::invalid_argument if point type is not supported for this
   * operation
   */
  bool transmit(std::shared_ptr<Object::DataPoint> point,
                CS101_CauseOfTransmission cause);

  /**
   * @brief send a message object to a remote client
   * @param message message that should be send via server
   * @param connection send to a single client identified via internal
   * connection object
   * @return information on operation success
   */
  bool send(std::shared_ptr<Remote::Message::OutgoingMessage> message,
            IMasterConnection connection = nullptr);

  /**
   * @brief Sends a batch of messages to connected clients
   *
   * This method is used to process and transmit a batch of data points
   * encapsulated within a `Remote::Message::Batch` object. The method verifies
   * various preconditions and constructs an ASDU (Application Service Data
   * Unit) for transmission. The ASDU is populated with information derived from
   * the batch and connection parameters, and is then sent to the master
   * connection, either with high or low priority, based on the cause of the
   * transmission. The method ensures that the ASDU and associated resources are
   * managed appropriately during this process.
   *
   * @param batch A shared pointer to the `Remote::Message::Batch` object
   *              containing the data points to be transmitted. The batch must
   *              have valid data points.
   * @param connection The master connection (`IMasterConnection`) through which
   *                   the batch will be sent. If null, the batch is processed
   * and sent in low-priority mode.
   * @return True if the batch is successfully processed and transmitted,
   *         otherwise false.
   */
  bool sendBatch(std::shared_ptr<Remote::Message::Batch> batch,
                 IMasterConnection connection = nullptr);

  /**
   * @brief Sends an activation confirmation ASDU to the master connection.
   *
   * This method is responsible for cloning incoming ASDUs into confirmation
   * messages to the master connection. The message can either confirm or reject
   * the activation request based on the specified parameters. If the ASDU is
   * intended for a global common address, it is sent to all connected stations.
   * Otherwise, it is sent directly to the requesting connection.
   *
   * @param connection The master connection to which the activation
   * confirmation will be sent.
   * @param asdu The ASDU (Application Service Data Unit) to be modified and
   * sent.
   * @param negative A flag indicating whether the confirmation is negative
   * (true) or positive (false).
   */
  void sendActivationConfirmation(IMasterConnection connection, CS101_ASDU asdu,
                                  bool negative = false);

  /**
   * @brief Sends an activation termination ASDU to the master connection.
   *
   * This method clones and tansforms the incoming ASDU with the Cause of
   * Transmission (COT) set to activation termination
   * (CS101_COT_ACTIVATION_TERMINATION). It accounts for global or specific
   * common addresses when sending the ASDU to the connected master station(s).
   *
   * @param connection The master connection through which the ASDU will be
   * sent.
   * @param asdu The Application Service Data Unit (ASDU) to be sent, modified
   *             within the function.
   */
  void sendActivationTermination(IMasterConnection connection, CS101_ASDU asdu);

  /**
   * @brief Sends an End of Initialization message (M_EI_NA_1) to the client.
   *
   * This method sends an End of Initialization (M_EI_NA_1) message using the
   * given common address and cause of initialization.
   * This is typically used to signal the completion of a configuration
   * and that some configurations got reset to initial values.
   *
   * @param commonAddress The common address (source) where the message is sent.
   *                      This is used to identify the originating system.
   * @param cause The cause of initialization (`CS101_CauseOfInitialization`)
   * that specifies the reason for sending the initialization message.
   */
  void sendEndOfInitialization(std::uint_fast16_t commonAddress,
                               CS101_CauseOfInitialization cause);

  /**
   * @brief Send periodic measurement broadcasts
   */
  void sendPeriodicInventory();

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
   * server
   * @param bind_ip ip-address the server should listen on for incoming client
   * requests
   * @param tcp_port port for listening to connections from clients
   * @param tick_rate_ms Interval in milliseconds between two cyclic measurement
   * transmissions
   * @param max_open_connections maximum number of allowed open connections
   * @param transport_security communication encryption instance reference
   */
  Server(const std::string &bind_ip, std::uint_fast16_t tcp_port,
         std::uint_fast16_t tick_rate_ms, std::uint_fast16_t select_timeout_ms,
         std::uint_fast8_t max_open_connections,
         std::shared_ptr<Remote::TransportSecurity> transport_security);

  /**
   * @brief Schedules timers for data points associated with servers' stations.
   *
   * This method iterates through all active client connections and their
   * respective stations. For each data point that has a timer set to execute
   * before the current time, it schedules a task to trigger the data point's
   * timer callback.
   */
  void scheduleDataPointTimer();

  /**
   * @brief Cleans up expired selections within the server.
   *
   * The `cleanupSelections` method removes outdated selections from the
   * server's selection list. This process iterates through the current
   * selections to determine if their lifetime has exceeded the configured
   * selection timeout. Removed selections will trigger an unselect operation.
   */
  void cleanupSelections();

  /**
   * @brief Removes all selections associated with the specified master
   * connection.
   *
   * This method iterates over the current selections and removes any selections
   * that are tied to the provided master connection.
   * Removed selections will NOT trigger an unselect operation, because the
   * connection is expected to be lost.
   *
   * @param connection The master connection whose associated selections are to
   * be removed.
   */
  void dropConnectionSelections(IMasterConnection connection);

  /**
   * @brief Cleans up a selection associated with the specified common address
   * (CA) and information object address (IOA).
   *
   * This method removes a selection from the selection vector based on the
   * provided CA and IOA. If a matching selection is found, it is unselected and
   * removed. Removed selections will trigger an unselect operation. This method
   * is used to delay the activation termination message after the actual
   * command response.
   *
   * @param ca The common address (CA) of the selection to be cleaned up.
   * @param ioa The information object address (IOA) of the selection to be
   * cleaned up.
   */
  void cleanupSelection(uint16_t ca, uint32_t ioa);

  /**
   * @brief Selects a control point for execution in the server.
   *
   * The select method identifies a specific control point based on the details
   * provided in the incoming message. If the selection criteria are met (i.e.,
   * the control point type is valid, or it is not time-expired), the selection
   * is registered or updated. If the control point is already selected and
   * conflicts with an existing selection, the method may reject the new
   * selection.
   *
   * @param connection The master connection that initiated the selection
   * process.
   * @param message A shared pointer to the incoming message containing control
   * point details such as type, originator address, common address, and
   * information object address.
   *
   * @return True if the selection was successfully created or updated; false
   * otherwise.
   */
  bool select(IMasterConnection connection,
              std::shared_ptr<Remote::Message::IncomingMessage> message);

  /**
   * @brief Cancels a previously made selection and performs necessary cleanup.
   *
   * The `unselect` method is responsible for terminating an active selection
   * initiated by a client. It schedules a task to send an activation
   * termination message to the originating connection and releases any
   * resources associated with the selection.
   *
   * @param selection The `Selection` object representing the active selection
   * to be canceled, including information about the master connection and ASDU.
   */
  void unselect(const Selection &selection);

  /**
   * @brief Executes a command on a specified data point based on an incoming
   * message.
   *
   * This method processes a command received from a remote source and executes
   * it on a target data point. In the case of select-and-execute command mode,
   * it ensures that the command was properly selected before execution. If the
   * selection conditions are not met, the command execution fails.
   * Additionally, it schedules cleanup tasks for selections once the command is
   * executed.
   *
   * @param connection A reference to the master connection initiating the
   * command.
   * @param message A shared pointer to the incoming message containing command
   * details.
   * @param point A shared pointer to the target data point to execute the
   * command on.
   * @return A `CommandResponseState` value indicating the result of the command
   * execution. Possible values are:
   *         - RESPONSE_STATE_FAILURE: Command execution failed.
   *         - RESPONSE_STATE_SUCCESS: Command execution succeeded.
   *         - RESPONSE_STATE_NONE: No response state determined.
   */
  CommandResponseState
  execute(IMasterConnection connection,
          std::shared_ptr<Remote::Message::IncomingMessage> message,
          std::shared_ptr<Object::DataPoint> point);

  /// @brief IP address of remote server
  const std::string ip{};

  ///< @brief Port of remote server
  const std::uint_fast16_t port = 0;

  /// @brief minimum interval between two periodic tasks
  const std::uint_fast16_t tickRate_ms{100};

  /// @brief selection init timestamp, to test against timeout
  const std::chrono::milliseconds selectTimeout_ms{100};

  /// @brief tls handler
  const std::shared_ptr<Remote::TransportSecurity> security{nullptr};

  /// @brief vector of stations accessible via this connection
  Object::StationVector stations{};

  /// @brief access mutex to lock station vector access
  mutable Module::GilAwareMutex station_mutex{"Server::station_mutex"};

  /// @brief lib60870-c slave struct
  CS104_Slave slave = nullptr;

  /// @brief state that defines if server thread should be running
  std::atomic_bool enabled{false};

  /// @brief server thread state
  std::atomic_bool running{false};

  /// @brief parameters of current server intance
  CS101_AppLayerParameters appLayerParameters;

  /// @brief MUTEX Lock to access connectionMap
  mutable Module::GilAwareMutex connection_mutex{"Server::connection_mutex"};

  /// @brief map of all connections to store connection state
  std::map<IMasterConnection, bool> connectionMap{};

  /// @brief MUTEX Lock to access selectionVEcotr
  mutable Module::GilAwareMutex selection_mutex{"Server::selection_mutex"};

  /// @brief vector of all selections
  std::vector<Selection> selectionVector{};

  /// @brief number of active connections
  std::atomic_uint_fast8_t activeConnections{0};

  /// @brief number of open connections
  std::atomic_uint_fast8_t openConnections{0};

  /// @brief maximum number of connections (0-255), 0 = no limit
  std::atomic_uint_fast8_t maxOpenConnections{0};

  std::priority_queue<Task> tasks;

  /// @brief server thread to execute periodic transmission
  std::thread *runThread = nullptr;

  /// @brief server thread mutex to not lock thread execution
  mutable std::mutex runThread_mutex{};

  /// @brief conditional variable to stop server thread without waiting for
  /// another loop
  mutable std::condition_variable_any runThread_wait{};

  /// @brief python callback function pointer
  Module::Callback<void> py_onReceiveRaw{
      "Server.on_receive_raw", "(server: c104.Server, data: bytes) -> None"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onSendRaw{
      "Server.on_send_raw", "(server: c104.Server, data: bytes) -> None"};

  /// @brief python callback function pointer
  Module::Callback<CommandResponseState> py_onClockSync{
      "Server.on_clock_sync", "(server: c104.Server, ip: str, date_time: "
                              "datetime.datetime) -> c104.ResponseState"};

  /// @brief python callback function pointer
  Module::Callback<void> py_onUnexpectedMessage{
      "Server.on_unexpected_message",
      "(server: c104.Server, message: c104.IncomingMessage, cause: c104.Umc) "
      "-> None"};

  /// @brief python callback function pointer
  Module::Callback<bool> py_onConnect{"Server.on_connect",
                                      "(server: c104.Server, ip: str) -> bool"};

  /// @brief server thread function
  void thread_run();

  /// @brief server thread function
  // @todo use callback thread
  // void thread_callback();

public:
  std::optional<uint8_t> getSelector(uint16_t ca, uint32_t ioa);

  /**
   * @brief Callback to accept or decline incoming client connections
   * @param parameter reference to custom bound connection data
   * @param ipAddress ip address of client
   * @return
   */
  static bool connectionRequestHandler(void *parameter, const char *ipAddress);

  /**
   * @brief Callback to handle connection state changes
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param event state change event (opened,closed,muted,unmuted identified via
   * constants)
   */
  static void connectionEventHandler(void *parameter,
                                     IMasterConnection connection,
                                     CS104_PeerConnectionEvent event);

  /**
   * @brief validate incoming ASDU, send negative response if invalid receiver
   * station or return ASDU wrapped in a IncomingMessage facade otherwise
   * @param connection reference to internal connection object
   * @param asdu incoming ASDU packet
   * @return
   */
  std::shared_ptr<Remote::Message::IncomingMessage>
  getValidMessage(IMasterConnection connection, CS101_ASDU asdu);

  /**
   * @brief Callback for logging incoming and outgoing byteStreams
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param msg pointer to first character of message
   * @param msgSize character count of message
   * @param sent direction of message
   * @warning DEBUG FUNCTION, IN ORDER TO ACTIVE THIS CALLBACK REMOVE COMMENT
   * WRAPPER IN CONSTRUCTOR
   */
  static void rawMessageHandler(void *parameter, IMasterConnection connection,
                                uint_fast8_t *msg, int msgSize, bool sent);

  /**
   * @brief Callback for reacting on incoming clock sync command and sending
   * responses
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param asdu incoming ASDU packet
   * @param newtime synchronized current timestamp formated as CP56Time2a
   * @return if command was handled.. if not it will be passed to asduHandler
   */
  // static bool clockSyncHandler(void *parameter, IMasterConnection connection,
  // CS101_ASDU asdu, CP56Time2a newtime);

  /**
   * @brief Callback for reacting on incoming interrogation command and sending
   * responses
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param asdu incoming ASDU packet
   * @param qoi which group of states and measurements are requested for
   * interrogation (group 1-16 or general represented in constants)
   * @return if command was handled.. if not it will be passed to asduHandler
   */
  static bool interrogationHandler(void *parameter,
                                   IMasterConnection connection,
                                   CS101_ASDU asdu,
                                   QualifierOfInterrogation qoi);

  /**
   * @brief Callback for reacting on incoming counter interrogation command and
   * sending responses
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param asdu incoming ASDU packet
   * @param qcc which group of counters are requested for interrogation (group
   * 1-4 or general represented in constants)
   * @return if command was handled.. if not it will be passed to asduHandler
   */
  static bool counterInterrogationHandler(void *parameter,
                                          IMasterConnection connection,
                                          CS101_ASDU asdu, QualifierOfCIC qcc);

  /**
   * @brief Callback for reacting on incoming read request command and sending
   * responses
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param asdu incoming ASDU packet
   * @param ioAddress unqiue identifier of information object whom's state/value
   * should be responded
   * @return if command was handled.. if not it will be passed to asduHandler
   */
  static bool readHandler(void *parameter, IMasterConnection connection,
                          CS101_ASDU asdu, int ioAddress);

  /**
   * @brief Callback for reacting on incoming commands
   * @param parameter reference to custom bound connection data
   * @param connection reference to internal connection object
   * @param asdu incoming ASDU packet
   * @return if command was handled.. if not server responds negative
   * confirmation to sender
   */
  static bool asduHandler(void *parameter, IMasterConnection connection,
                          CS101_ASDU asdu);

  /**
   * @brief Generates a string representation of the server instance.
   *
   * This method provides a detailed description of the server instance,
   * including its IP address, port number, the number of connected clients, the
   * number of associated stations, and the memory address of the server object.
   *
   * @return A string containing the server's state and attributes.
   */
  std::string toString() const {
    size_t lencon = 0;
    {
      std::scoped_lock<Module::GilAwareMutex> const lock(connection_mutex);
      lencon = connectionMap.size();
    }
    size_t lenst = 0;
    {
      std::scoped_lock<Module::GilAwareMutex> const lock(station_mutex);
      lenst = stations.size();
    }
    std::ostringstream oss;
    oss << "<104.Server ip=" << ip << ", port=" << std::to_string(port)
        << ", #clients=" << std::to_string(lencon)
        << ", #stations=" << std::to_string(lenst) << " at " << std::hex
        << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  };
};

#endif // C104_SERVER_H
