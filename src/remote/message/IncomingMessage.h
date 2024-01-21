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
 * @file IncomingMessage.h
 * @brief create a message object from ASDU struct
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_INCOMINGMESSAGE_H
#define C104_REMOTE_MESSAGE_INCOMINGMESSAGE_H

#include "IMessageInterface.h"
#include "module/GilAwareMutex.h"

namespace Remote {

namespace Message {

/**
 * @brief facade model to read incoming messages (ASDU packages)
 */
class IncomingMessage : public IMessageInterface,
                        public std::enable_shared_from_this<IncomingMessage> {
public:
  /**
   * @brief Create an IncomingMessage as facade pattern to access an incoming
   * CS101_ASDU packet via object oriented methods
   * @param packet internal incoming message
   * @param app_layer_parameters connection parameters
   * @throws std::invalid_argument if information value is incompatible with
   * information type
   */
  [[nodiscard]] static std::shared_ptr<IncomingMessage>
  create(CS101_ASDU packet, CS101_AppLayerParameters app_layer_parameters) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<IncomingMessage>(
        new IncomingMessage(packet, app_layer_parameters));
  }

  /**
   * @brief free extracted information object
   */
  ~IncomingMessage();

  /**
   * @brief Getter for internal ASDU packet
   * @return ASDU packet
   */
  CS101_ASDU getAsdu() const;

  /**
   * @brief Getter for raw bytes
   * @return bytes
   */
  unsigned char *getRawBytes() const;

  /**
   * @brief Getter for raw bytes text explanation
   * @return string
   */
  std::string getRawMessageString() const;

  /**
   * @brief Get the number of information objects inside this message
   * @return count
   */
  std::uint_fast8_t getNumberOfObject() const;

  /**
   * @brief Extract the first information object contained in this message
   */
  void first();

  /**
   * @brief Extract the next information object contained in this message
   * @return information on operation success: is a new information object
   * extracted and available
   */
  bool next();

  std::uint_fast64_t getUpdatedAt() const;

  /**
   * @brief test if cause of transmission is compatible with information type
   * @return if cause is valid
   * @throws std::invalid_argument if feature is not yet implemented for that
   * information type
   */
  bool isValidCauseOfTransmission() const;

  /**
   * @brief test if message is a command and requires a confirmation (ACK)
   */
  bool requireConfirmation() const;

  /**
   * @brief test if message is a command with select flag set
   */
  bool isSelectCommand() const;

private:
  /**
   * @brief Create an IncomingMessage as facade pattern to access an incoming
   * CS101_ASDU packet via object oriented methods
   * @param packet internal incoming message
   * @param app_layer_parameters connection parameters
   * @throws std::invalid_argument if information value is incompatible with
   * information type
   */
  explicit IncomingMessage(CS101_ASDU packet,
                           CS101_AppLayerParameters app_layer_parameters);

  /// @brief IEC60870-5-104 asdu struct
  CS101_ASDU asdu{nullptr};

  CS101_AppLayerParameters parameters{nullptr};

  ///< @brief MUTEX Lock to change extracted information object position
  mutable Module::GilAwareMutex position_mutex{
      "IncomingMessage::position_mutex"};

  /// @brief currently extracted information object position starting from zero
  std::atomic_uint_fast8_t position{0};

  /// @brief state that describes if position got resetted to first
  std::atomic_bool positionReset{true};

  /// @brief state that describes if current position is a valid position
  /// (0<=positon<number of information objects)
  std::atomic_bool positionValid{false};

  /// @brief number of available information objects inside this message
  std::atomic_uint_fast8_t numberOfObject{0};

  /// @brief timestamp in milliseconds since latest value update
  std::atomic_uint_fast64_t updatedAt{0};

  /// @brief state that describes if a command results in a SELECT or an EXECUTE
  /// action
  std::atomic_bool selectFlag{false};

  /**
   * @brief extract meta data from this message: commonAddress,
   * originatorAddress, message identifier and mode, ...
   * @throws std::invalid_argument if information value is incompatible with
   * information type
   */
  void extractMetaData();

  /**
   * @brief extract values of an information object at the current position
   */
  void extractInformationObject();
};
} // namespace Message
} // namespace Remote

#endif // C104_REMOTE_MESSAGE_INCOMINGMESSAGE_H
