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
 * @file IMessageInterface.h
 * @brief 60870-5-104 message interface
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_IMESSAGEINTERFACE_H
#define C104_REMOTE_MESSAGE_IMESSAGEINTERFACE_H

#include "module/GilAwareMutex.h"
#include "types.h"

/**
 * @brief Remote layer - remote communication via IEC60870-5-104
 */
namespace Remote {

/**
 * @brief Remote::Message layer - remote message objects received send via
 * IEC60870-5-104
 */
namespace Message {

/**
 * @brief interface for Remote::Message::IncomingMessage and
 * Remote::Message::OutgoingMessage and derived objects (Remote::Command,
 * Remote::Report)
 */
class IMessageInterface {
public:
  // noncopyable
  IMessageInterface(const IMessageInterface &) = delete;
  IMessageInterface &operator=(const IMessageInterface &) = delete;

  /**
   * @brief Getter for remote message type identifier
   * @return type identifier as enum
   */
  virtual IEC60870_5_TypeID getType() const { return type.load(); }

  /**
   * @brief Getter for common address of remote message receiver
   * @return common address of receiver station
   */
  virtual uint_fast16_t getCommonAddress() const {
    return commonAddress.load();
  }

  /**
   * @brief Getter for originator address of client who is related to remote
   * message
   * @return common address of client
   */
  virtual uint_fast8_t getOriginatorAddress() const {
    return originatorAddress.load();
  }

  /**
   * @brief Getter for information object address
   * @return address of information object related to message content
   */
  virtual uint_fast32_t getIOA() const {
    return informationObjectAddress.load();
  }

  /**
   * @brief Getter for information object of remote message
   * @return internal informationObject
   */
  virtual InformationObject getInformationObject() const {
    std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

    return io;
  }

  /**
   * @brief Get the value from an information object inside the remote message
   * @return value as double
   */
  virtual double getValue() const { return value.load(); }

  /**
   * @brief test if message test flag is set
   * @return information if message has a value
   */
  virtual bool isTest() const { return test.load(); }

  /**
   * @brief test if message negative flag is set
   * @return information if message has a value
   */
  virtual bool isNegative() const { return negative.load(); }

  /**
   * @brief test if message sequence flag is set
   * @return information if message has a value
   */
  virtual bool isSequence() const { return sequence.load(); }

  /**
   * @brief Test if message has a connectionString of connection used for this
   * message
   * @return information if message has a connectionString
   */
  virtual bool hasConnectionString() const {
    std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

    return !connectionString.empty();
  }

  /**
   * @brief Getter for connectionString of connection used for this message
   * @return ip:port connectionString
   */
  virtual std::string getConnectionString() const {
    std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

    return connectionString;
  }

  /**
   * @brief Setter for connectionString of connection used for this message
   * @param s ip:port connectionString
   */
  virtual void setConnectionString(const std::string &s) {
    std::lock_guard<Module::GilAwareMutex> const lock(access_mutex);

    connectionString = s;
  }

  /**
   * @brief Getter for cause of transmission: why was this message transmitted
   * @return cause of transmission as enum
   */
  virtual CS101_CauseOfTransmission getCauseOfTransmission() const {
    return causeOfTransmission.load();
  }

  /**
   * @brief Getter for quality of message information
   * @return quality as bitset object
   */
  virtual Quality getQuality() const { return quality.load(); }

protected:
  IMessageInterface() = default;

  /// @brief MUTEX Lock for non-atomic member access
  mutable Module::GilAwareMutex access_mutex{"IMessageInterface::access_mutex"};

  /// @brief IEC60870-5-104 message type identifier
  std::atomic<IEC60870_5_TypeID> type{C_TS_TA_1};

  /// @brief IEC60870-5-104 receiver station common address
  std::atomic_uint_fast16_t commonAddress{0};

  /// @brief IEC60870-5-104 related client address
  std::atomic_uint_fast8_t originatorAddress{0};

  /// @brief IEC60870-5-104 unique identifier of related information object
  std::atomic_uint_fast32_t informationObjectAddress{0};

  /// @brief IEC60870-5-104 internal information object
  InformationObject io{nullptr};

  /// @brief IEC60870-5-104 cause of transmission: why was message transmitted
  std::atomic<CS101_CauseOfTransmission> causeOfTransmission{
      CS101_COT_UNKNOWN_COT};

  /// @brief IEC60870-5-104 describes the quality of the information
  std::atomic<Quality> quality{Quality::None};

  /// @brief ip:port connectionString of the used connection
  std::string connectionString{};

  /// @brief value of the informationObject
  std::atomic<double> value{-1};

  /// @brief state that defines if informationObject has a value
  std::atomic_bool test{false};

  /// @brief state that defines if informationObject has a value
  std::atomic_bool negative{false};

  /// @brief state that defines if informationObject has a value
  std::atomic_bool sequence{false};
};
} // namespace Message

} // namespace Remote

#endif // C104_REMOTE_MESSAGE_IMESSAGEINTERFACE_H
