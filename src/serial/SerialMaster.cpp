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
 * @file SerialMaster.cpp
 * @brief IEC 60870-5-101 master implementation
 *
 * @package iec104-python
 * @namespace Serial
 *
 */

#include "serial/SerialMaster.h"
#include "module/ScopedGilAcquire.h"
#include "module/ScopedGilRelease.h"

#include <chrono>

namespace Serial {

SerialMaster::SerialMaster(const std::string &port, int baud_rate, Parity parity,
                           int data_bits, int stop_bits, LinkLayerMode link_mode)
    : portPath(port), baudRate(baud_rate), parity(parity),
      dataBits(data_bits), stopBits(stop_bits), linkMode(link_mode) {

  // Initialize link layer parameters with defaults
  llParams.addressLength = 1;
  llParams.timeoutForAck = 200;
  llParams.timeoutRepeat = 1000;
  llParams.useSingleCharACK = true;
  llParams.timeoutLinkState = 3000;

  // Initialize application layer parameters with defaults
  alParams.sizeOfTypeId = 1;
  alParams.sizeOfVSQ = 1;
  alParams.sizeOfCOT = 2;
  alParams.originatorAddress = 0;
  alParams.sizeOfCA = 2;
  alParams.sizeOfIOA = 3;
  alParams.maxSizeOfASDU = 249;

  // Create serial port
  char parityChar = static_cast<char>(parity);
  serialPort = SerialPort_create(port.c_str(), baud_rate, data_bits, parityChar, stop_bits);

  if (!serialPort) {
    throw std::runtime_error("Failed to create serial port: " + port);
  }

  // Open serial port
  if (!SerialPort_open(serialPort)) {
    SerialPort_destroy(serialPort);
    serialPort = nullptr;
    throw std::runtime_error("Failed to open serial port: " + port);
  }

  // Create CS101 master
  IEC60870_LinkLayerMode mode = static_cast<IEC60870_LinkLayerMode>(link_mode);
  master = CS101_Master_create(serialPort, &llParams, &alParams, mode);

  if (!master) {
    SerialPort_close(serialPort);
    SerialPort_destroy(serialPort);
    serialPort = nullptr;
    throw std::runtime_error("Failed to create CS101 master");
  }

  // Set up callbacks
  CS101_Master_setASDUReceivedHandler(master, asduReceivedHandler, this);
  CS101_Master_setLinkLayerStateChanged(master, linkLayerStateChanged, this);
  CS101_Master_setRawMessageHandler(master, rawMessageHandler, this);
}

SerialMaster::~SerialMaster() {
  stop();

  if (master) {
    CS101_Master_destroy(master);
    master = nullptr;
  }

  if (serialPort) {
    SerialPort_close(serialPort);
    SerialPort_destroy(serialPort);
    serialPort = nullptr;
  }
}

void SerialMaster::start() {
  if (running.load()) {
    return;
  }

  running.store(true);

  // Start the master's internal thread
  CS101_Master_start(master);

  // Also start our own polling thread for unbalanced mode
  if (linkMode == LinkLayerMode::UNBALANCED) {
    runThread = new std::thread(&SerialMaster::thread_run, this);
  }
}

void SerialMaster::stop() {
  if (!running.load()) {
    return;
  }

  running.store(false);

  // Stop the master's internal thread
  CS101_Master_stop(master);

  // Stop our polling thread
  {
    std::lock_guard<std::mutex> lock(runThread_mutex);
    runThread_wait.notify_all();
  }

  if (runThread && runThread->joinable()) {
    runThread->join();
    delete runThread;
    runThread = nullptr;
  }
}

bool SerialMaster::isRunning() const {
  return running.load();
}

void SerialMaster::addSlave(int address) {
  CS101_Master_addSlave(master, address);
}

void SerialMaster::pollSlave(int address) {
  CS101_Master_pollSingleSlave(master, address);
}

void SerialMaster::useSlaveAddress(int address) {
  CS101_Master_useSlaveAddress(master, address);
}

void SerialMaster::sendInterrogationCommand(int common_address, int qoi) {
  CS101_Master_sendInterrogationCommand(
      master, CS101_COT_ACTIVATION, common_address,
      static_cast<QualifierOfInterrogation>(qoi));
}

void SerialMaster::sendCounterInterrogationCommand(int common_address, uint8_t qcc) {
  CS101_Master_sendCounterInterrogationCommand(master, CS101_COT_ACTIVATION, common_address, qcc);
}

void SerialMaster::sendReadCommand(int common_address, int ioa) {
  CS101_Master_sendReadCommand(master, common_address, ioa);
}

void SerialMaster::sendClockSyncCommand(int common_address) {
  // Get current time as milliseconds since epoch
  auto now = std::chrono::system_clock::now();
  auto ms_since_epoch = std::chrono::duration_cast<std::chrono::milliseconds>(
      now.time_since_epoch()).count();

  struct sCP56Time2a cpTime;
  CP56Time2a_setFromMsTimestamp(&cpTime, static_cast<uint64_t>(ms_since_epoch));

  CS101_Master_sendClockSyncCommand(master, common_address, &cpTime);
}

void SerialMaster::setOnReceiveCallback(py::object &callable) {
  py_onReceive.reset(callable);
}

void SerialMaster::setOnLinkStateChangeCallback(py::object &callable) {
  py_onLinkStateChange.reset(callable);
}

void SerialMaster::setOnRawMessageCallback(py::object &callable) {
  py_onRawMessage.reset(callable);
}

void SerialMaster::thread_run() {
  while (running.load()) {
    // In unbalanced mode, periodically run the master state machine
    CS101_Master_run(master);

    std::unique_lock<std::mutex> lock(runThread_mutex);
    runThread_wait.wait_for(lock, std::chrono::milliseconds(10));
  }
}

bool SerialMaster::asduReceivedHandler(void *parameter, int address, CS101_ASDU asdu) {
  auto *self = static_cast<SerialMaster *>(parameter);

  if (self->py_onReceive.is_set()) {
    // Get ASDU info
    int typeId = CS101_ASDU_getTypeID(asdu);
    int ca = CS101_ASDU_getCA(asdu);
    int cot = CS101_ASDU_getCOT(asdu);
    int numElements = CS101_ASDU_getNumberOfElements(asdu);

    // Get payload as bytes
    int payloadSize = CS101_ASDU_getPayloadSize(asdu);
    uint8_t* payload = CS101_ASDU_getPayload(asdu);

    Module::ScopedGilAcquire const acquire("SerialMaster.on_receive");

    // Create a dict with ASDU info
    py::dict asdu_info;
    asdu_info["type_id"] = typeId;
    asdu_info["common_address"] = ca;
    asdu_info["cot"] = cot;
    asdu_info["num_elements"] = numElements;
    if (payload && payloadSize > 0) {
      asdu_info["payload"] = py::bytes(reinterpret_cast<char*>(payload), payloadSize);
    }

    try {
      self->py_onReceive.call(self->shared_from_this(), asdu_info);
    } catch (const py::error_already_set &e) {
      // Log error but don't propagate
    }
  }

  return true; // ASDU handled
}

void SerialMaster::linkLayerStateChanged(void *parameter, int address,
                                          LinkLayerState state) {
  auto *self = static_cast<SerialMaster *>(parameter);

  if (self->py_onLinkStateChange.is_set()) {
    std::string stateStr;
    switch (state) {
    case LL_STATE_IDLE:
      stateStr = "IDLE";
      break;
    case LL_STATE_ERROR:
      stateStr = "ERROR";
      break;
    case LL_STATE_BUSY:
      stateStr = "BUSY";
      break;
    case LL_STATE_AVAILABLE:
      stateStr = "AVAILABLE";
      break;
    default:
      stateStr = "UNKNOWN";
      break;
    }

    Module::ScopedGilAcquire const acquire("SerialMaster.on_link_state_change");
    try {
      self->py_onLinkStateChange.call(self->shared_from_this(), address, stateStr);
    } catch (const py::error_already_set &e) {
      // Log error but don't propagate
    }
  }
}

void SerialMaster::rawMessageHandler(void *parameter, uint8_t *msg, int msgSize, bool sent) {
  auto *self = static_cast<SerialMaster *>(parameter);

  if (self->py_onRawMessage.is_set()) {
    Module::ScopedGilAcquire const acquire("SerialMaster.on_raw_message");
    py::bytes msg_bytes(reinterpret_cast<char *>(msg), msgSize);

    try {
      self->py_onRawMessage.call(self->shared_from_this(), msg_bytes, sent);
    } catch (const py::error_already_set &e) {
      // Log error but don't propagate
    }
  }
}

} // namespace Serial
