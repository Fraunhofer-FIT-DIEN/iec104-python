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
 * @file OutgoingMessage.h
 * @brief abstract base class for outgoing ASDU messages
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_OUTGOINGMESSAGE_H
#define C104_REMOTE_MESSAGE_OUTGOINGMESSAGE_H

#include "IMessageInterface.h"

namespace Remote {
namespace Message {

/**
 * @brief model to modify and transmit Remote::Command and Remote::Report
 * objects
 * @todo add support for packed messages / multiple IOs in outgoing message
 */
class OutgoingMessage : public IMessageInterface,
                        public std::enable_shared_from_this<OutgoingMessage> {
public:
  /**
   * @brief remove object
   */
  ~OutgoingMessage();

  /**
   * @brief Setter for originator address of outgoing message
   * @param address address that identifies the client related to this message
   */
  void setOriginatorAddress(uint_fast8_t address);

  // CAUSE OF TRANSMISSION

  /**
   * @brief Setter for cause of transmission: why should this message be send
   * @param cause of transmission as enum
   */
  void setCauseOfTransmission(CS101_CauseOfTransmission cause);

  /**
   * @brief Setter for cause of transmission: Mark this message as periodic
   */
  void setIsPeriodicTransmission();

  /**
   * @brief Setter for cause of transmission: Mark this message as spontaneous
   */
  void setIsSpontaneousTransmission();

  /**
   * @brief Setter for cause of transmission: Mark this message as requested by
   * client
   */
  void setIsRequestedTransmission();

  // QUALITY

  /**
   * @brief Setter for quality of information
   * @param descriptor quality as enum
   */
  void setQuality(Quality descriptor = Quality::None);

  // IS_SENT

  /**
   * @brief Test if message was sent
   * @return information if message was sent
   */
  bool getIsSent() const;

  /**
   * @brief Send the message via client or specific server connection
   * @param master internal connection reference used to send a message via
   * server to a single client
   * @return information on success
   * @throws std::invalid_argument if server or connection reference is invalid
   */
  bool send(IMasterConnection master = nullptr);

  std::shared_ptr<Server> getServer();

  std::shared_ptr<Connection> getConnection();

  // SUCCESS

  /**
   * @brief Get information on success
   * @return information on success
   */
  bool wasSuccessful() { return success; }

protected:
  /**
   * @brief Create an outgoing message that should be send via client or server
   * @param ca receiver station common address
   * @throws std::invalid_argument if point reference, connection or server
   * reference is invalid
   */
  explicit OutgoingMessage(uint_fast16_t ca);

  /**
   * @brief Create an outgoing message that should be send via client or server
   * to a given DataPoint
   * @param point point that defines the receiver and related information of the
   * outgoing message
   */
  explicit OutgoingMessage(std::shared_ptr<Object::DataPoint> point);

  /// @brief state that defines if message was sent via client or server
  std::atomic_bool sent{false};

  /// @brief state that defines if message was received successfully and if
  /// client message was executed successfully
  std::atomic_bool success{false};

  /// @brief reference to related server connection (if server)
  std::weak_ptr<Server> server{};

  /// @brief reference to related client connection (if client)
  std::weak_ptr<Connection> connection{};
};

} // namespace Message
} // namespace Remote

#endif // C104_REMOTE_MESSAGE_OUTGOINGMESSAGE_H
