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
#include "remote/message/IncomingMessage.h"

/**
 * @brief service model for IEC60870-5-104 communication as server
 */
class Server : public std::enable_shared_from_this<Server> {

  // @todo import/export station with DataPoints to string
public:
  // noncopyable
  Server(const Server &) = delete;
  Server &operator=(const Server &) = delete;

  [[nodiscard]] static std::shared_ptr<Server> create(
      std::string bind_ip = "0.0.0.0",
      uint_fast16_t tcp_port = IEC_60870_5_104_DEFAULT_PORT,
      uint_fast32_t tick_rate_ms = 1000,
      std::uint_fast8_t max_open_connections = 0,
      std::shared_ptr<Remote::TransportSecurity> transport_security = nullptr) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Server>(new Server(bind_ip, tcp_port, tick_rate_ms,
                                              max_open_connections,
                                              std::move(transport_security)));
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

  void setMaxOpenConnections(std::uint_fast8_t max_open_connections);

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
   * @brief set python callback that will be executed on incoming clock sync
   * command
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnClockSyncCallback(py::object &callable);

  CommandResponseState onClockSync(std::string _ip, CP56Time2a time);

  /**
   * @brief set python callback that will be executed on unexpected incoming
   * messages
   * @throws std::invalid_argument if callable signature does not match
   */
  void setOnUnexpectedMessageCallback(py::object &callable);

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

  void sendActivationConfirmation(IMasterConnection connection, CS101_ASDU asdu,
                                  bool negative = false);

  void sendActivationTermination(IMasterConnection connection, CS101_ASDU asdu);

  /**
   * @brief Send full interrogation response or periodic measurement broadcasts
   * @param cot cause of transmission: why this message should be send
   * @param commonAddress send interrogation for a single station only
   * identified via common address
   * @param connection send to a single client identified via internal
   * connection object
   */
  void sendInterrogationResponse(
      CS101_CauseOfTransmission cot,
      uint_fast16_t commonAddress = IEC60870_GLOBAL_COMMON_ADDRESS,
      IMasterConnection connection = nullptr);
  /*
      void sendCounterInterrogationResponse(CS101_CauseOfTransmission cot,
     uint_fast16_t commonAddress = IEC60870_GLOBAL_COMMON_ADDRESS,
     IMasterConnection connection = nullptr);

      void sendPeriodic(uint_fast16_t commonAddress =
     IEC60870_GLOBAL_COMMON_ADDRESS, IMasterConnection connection = nullptr);
  */

private:
  /**
   * @brief Create a new remote connection handler instance that acts as a
   * server
   * @param bind_ip ip-address the server should listen on for incoming client requests
   * @param tcp_port port for listening to connections from clients
   * @param tick_rate_ms Interval in milliseconds between two cyclic measurement transmissions
   * @param max_open_connections maximum number of allowed open connections
   * @param transport_security communication encryption instance reference
   */
  Server(const std::string &bind_ip, std::uint_fast16_t tcp_port,
         std::uint_fast32_t tick_rate_ms,
         std::uint_fast8_t max_open_connections,
         std::shared_ptr<Remote::TransportSecurity> transport_security);

  /// @brief IP address of remote server
  std::string ip{};

  ///< @brief Port of remote server
  std::uint_fast16_t port = 0;

  /// @brief tls handler
  std::shared_ptr<Remote::TransportSecurity> security{nullptr};

  /// @brief vector of stations accessible via this connection
  Object::StationVector stations{};

  /// @brief access mutex to lock station vector access
  mutable Module::GilAwareMutex station_mutex{"Server::station_mutex"};

  /// @brief lib60870-c slave struct
  CS104_Slave slave = nullptr;

  /// @brief state that defines if server thread should be running
  std::atomic_bool enabled{false};

  /// @brief minimum interval between to periodic broadcasts in milliseconds
  std::atomic_uint_fast32_t tickRate_ms{1000};

  /// @brief parameters of current server intance
  CS101_AppLayerParameters appLayerParameters;

  /// @brief MUTEX Lock to access connection_mutex
  mutable Module::GilAwareMutex connection_mutex{"Server::connection_mutex"};

  /// @brief map of all connections to store connection state
  std::map<IMasterConnection, bool> connectionMap{};

  /// @brief number of active connections
  std::atomic_uint_fast8_t activeConnections{0};

  /// @brief number of open connections
  std::atomic_uint_fast8_t openConnections{0};

  /// @brief maximum number of connections (0-255), 0 = no limit
  std::atomic_uint_fast8_t maxOpenConnections{0};

  /// @brief server thread state
  std::atomic_bool running{false};

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
};

#endif // C104_SERVER_H
