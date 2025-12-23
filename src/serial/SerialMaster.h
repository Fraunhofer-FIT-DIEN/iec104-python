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
 * @file SerialMaster.h
 * @brief IEC 60870-5-101 master (client) for serial communication
 *
 * @package iec104-python
 * @namespace Serial
 *
 */

#ifndef C104_SERIAL_MASTER_H
#define C104_SERIAL_MASTER_H

#include "types.h"

#include "module/Callback.h"
#include "module/GilAwareMutex.h"
#include "object/Station.h"

// lib60870 headers (C API) - order matters!
extern "C" {
#include "hal_serial.h"
#include "link_layer_parameters.h"
#include "iec60870_common.h"
#include "cs101_master.h"
}

namespace Serial {

/**
 * @brief Link layer mode for IEC 101 communication
 */
enum class LinkLayerMode {
  BALANCED = IEC60870_LINK_LAYER_BALANCED,
  UNBALANCED = IEC60870_LINK_LAYER_UNBALANCED
};

/**
 * @brief Serial port parity setting
 */
enum class Parity {
  NONE = 'N',
  EVEN = 'E',
  ODD = 'O'
};

/**
 * @brief IEC 60870-5-101 master for serial communication
 */
class SerialMaster : public std::enable_shared_from_this<SerialMaster> {

public:
  // noncopyable
  SerialMaster(const SerialMaster &) = delete;
  SerialMaster &operator=(const SerialMaster &) = delete;

  /**
   * @brief Factory method to create a shared pointer to a SerialMaster instance.
   *
   * @param port Serial port path (e.g. "/dev/ttyUSB0" or "COM1")
   * @param baud_rate Baud rate (e.g. 9600, 19200, 115200)
   * @param parity Parity setting (NONE, EVEN, ODD)
   * @param data_bits Number of data bits (usually 8)
   * @param stop_bits Number of stop bits (usually 1)
   * @param link_mode Link layer mode (BALANCED or UNBALANCED)
   * @return A shared pointer to the newly created SerialMaster instance.
   */
  [[nodiscard]] static std::shared_ptr<SerialMaster> create(
      const std::string &port,
      int baud_rate = 9600,
      Parity parity = Parity::EVEN,
      int data_bits = 8,
      int stop_bits = 1,
      LinkLayerMode link_mode = LinkLayerMode::UNBALANCED) {
    return std::shared_ptr<SerialMaster>(
        new SerialMaster(port, baud_rate, parity, data_bits, stop_bits, link_mode));
  }

  /**
   * @brief Destructor - clean up resources
   */
  ~SerialMaster();

  /**
   * @brief Start the master communication thread
   */
  void start();

  /**
   * @brief Stop the master communication thread
   */
  void stop();

  /**
   * @brief Check if master is currently running
   * @return true if running
   */
  bool isRunning() const;

  /**
   * @brief Add a slave to communicate with
   * @param address Link layer address of the slave
   */
  void addSlave(int address);

  /**
   * @brief Poll a slave for data (unbalanced mode only)
   * @param address Link layer address of the slave to poll
   */
  void pollSlave(int address);

  /**
   * @brief Set the slave address for subsequent commands
   * @param address Link layer address
   */
  void useSlaveAddress(int address);

  /**
   * @brief Send general interrogation command
   * @param common_address Common address of ASDU
   * @param qoi Qualifier of interrogation (20 = station interrogation)
   */
  void sendInterrogationCommand(int common_address, int qoi = 20);

  /**
   * @brief Send counter interrogation command
   * @param common_address Common address of ASDU
   * @param qcc Qualifier of counter interrogation
   */
  void sendCounterInterrogationCommand(int common_address, uint8_t qcc = 0x05);

  /**
   * @brief Send read command
   * @param common_address Common address of ASDU
   * @param ioa Information object address
   */
  void sendReadCommand(int common_address, int ioa);

  /**
   * @brief Send clock synchronization command
   * @param common_address Common address of ASDU
   */
  void sendClockSyncCommand(int common_address);

  /**
   * @brief Set callback for received ASDUs
   */
  void setOnReceiveCallback(py::object &callable);

  /**
   * @brief Set callback for link layer state changes
   */
  void setOnLinkStateChangeCallback(py::object &callable);

  /**
   * @brief Set callback for raw messages (debug)
   */
  void setOnRawMessageCallback(py::object &callable);

  /**
   * @brief Get the serial port path
   */
  std::string getPort() const { return portPath; }

  /**
   * @brief Get the baud rate
   */
  int getBaudRate() const { return baudRate; }

  /**
   * @brief Get the link layer mode
   */
  LinkLayerMode getLinkMode() const { return linkMode; }

  /**
   * @brief String representation
   */
  std::string toString() const {
    std::ostringstream oss;
    oss << "<101.SerialMaster port=" << portPath
        << ", baud=" << baudRate
        << ", mode=" << (linkMode == LinkLayerMode::BALANCED ? "balanced" : "unbalanced")
        << ", running=" << (running.load() ? "true" : "false")
        << " at " << std::hex << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  }

private:
  /**
   * @brief Private constructor
   */
  SerialMaster(const std::string &port, int baud_rate, Parity parity,
               int data_bits, int stop_bits, LinkLayerMode link_mode);

  /// @brief Serial port path
  std::string portPath;

  /// @brief Baud rate
  int baudRate;

  /// @brief Parity setting
  Parity parity;

  /// @brief Data bits
  int dataBits;

  /// @brief Stop bits
  int stopBits;

  /// @brief Link layer mode
  LinkLayerMode linkMode;

  /// @brief lib60870 serial port handle
  SerialPort serialPort{nullptr};

  /// @brief lib60870 CS101 master handle
  CS101_Master master{nullptr};

  /// @brief Link layer parameters
  struct sLinkLayerParameters llParams;

  /// @brief Application layer parameters
  struct sCS101_AppLayerParameters alParams;

  /// @brief Running state
  std::atomic_bool running{false};

  /// @brief Thread for master operations
  std::thread *runThread{nullptr};

  /// @brief Mutex for thread control
  mutable std::mutex runThread_mutex{};

  /// @brief Condition variable for thread control
  mutable std::condition_variable runThread_wait{};

  /// @brief Python callback for received ASDUs
  Module::Callback<void> py_onReceive{
      "SerialMaster.on_receive",
      "(master: c104.SerialMaster, asdu: dict) -> None"};

  /// @brief Python callback for link state changes
  Module::Callback<void> py_onLinkStateChange{
      "SerialMaster.on_link_state_change",
      "(master: c104.SerialMaster, address: int, state: str) -> None"};

  /// @brief Python callback for raw messages
  Module::Callback<void> py_onRawMessage{
      "SerialMaster.on_raw_message",
      "(master: c104.SerialMaster, data: bytes, is_sent: bool) -> None"};

  /// @brief Thread function
  void thread_run();

  /// @brief Static callback for received ASDUs
  static bool asduReceivedHandler(void *parameter, int address, CS101_ASDU asdu);

  /// @brief Static callback for link layer state changes
  static void linkLayerStateChanged(void *parameter, int address, LinkLayerState state);

  /// @brief Static callback for raw messages
  static void rawMessageHandler(void *parameter, uint8_t *msg, int msgSize, bool sent);
};

} // namespace Serial

#endif // C104_SERIAL_MASTER_H
