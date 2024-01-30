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
 * @file PointCommand.h
 * @brief create an outgoing message from data point in control direction
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_POINTCOMMAND_H
#define C104_REMOTE_MESSAGE_POINTCOMMAND_H

#include "OutgoingMessage.h"

namespace Remote {
namespace Message {

/**
 * @brief IEC60870-5 generic data point information message
 * @details Valid cause of transmission: x
 * @deprecated since all used messages are implemented in their own classes a
 * generic point information should not be used anymore
 */
class PointCommand : public OutgoingMessage {
public:
  /**
   * @brief Create a message for a certain DataPoint, type of message is
   * identified via DataPoint
   * @param point point whom's value should be reported to remote client
   * @param select flag for select and execute command mode (lock control
   * access)
   * @throws std::invalid_argument if point reference, connection or server
   * reference is invalid
   */
  [[nodiscard]] static std::shared_ptr<PointCommand>
  create(std::shared_ptr<Object::DataPoint> point, const bool select = false) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<PointCommand>(
        new PointCommand(std::move(point), select));
  }

  /**
   * @brief free informationObject after use
   */
  ~PointCommand();

private:
  /**
   * @brief Create a message for a certain DataPoint, type of message is
   * identified via DataPoint
   * @param point point whom's value should be reported to remote client
   * @param select flag for select and execute command mode (lock control
   * access)
   * @throws std::invalid_argument if point reference, connection or server
   * reference is invalid
   */
  explicit PointCommand(std::shared_ptr<Object::DataPoint> point, bool select);

  /// @brief timestamp of measurement in milliseconds
  uint_fast64_t updated_at;

  /// @brief duration of event if required
  sCP16Time2a duration;

  /// @brief timestamp of measurement formatted as CP56Time2a
  sCP56Time2a time;
};
} // namespace Message

} // namespace Remote

#endif // C104_REMOTE_MESSAGE_POINTCOMMAND_H
