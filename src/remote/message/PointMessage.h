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
 * @file PointInformation.h
 * @brief create an outgoing from data point in monitoring direction
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_POINTMESSAGE_H
#define C104_REMOTE_MESSAGE_POINTMESSAGE_H

#include "OutgoingMessage.h"

namespace Remote {
namespace Message {

/**
 * @brief IEC60870-5 generic data point information message
 * @details Valid cause of transmission: x
 * generic point information should not be used anymore
 */
class PointMessage : public OutgoingMessage {
public:
  [[nodiscard]] static std::shared_ptr<PointMessage>
  create(std::shared_ptr<Object::DataPoint> point) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<PointMessage>(new PointMessage(std::move(point)));
  }

  /**
   * @brief free informationObject after use
   */
  ~PointMessage();

private:
  /**
   * @brief Create a message for a certain DataPoint, type of message is
   * identified via DataPoint
   * @param point point whom's value should be reported to remote client
   */
  explicit PointMessage(std::shared_ptr<Object::DataPoint> point);

  /// @brief timestamp of measurement in milliseconds
  uint_fast64_t updated_at;

  /// @brief duration of event if required
  sCP16Time2a duration;

  /// @brief timestamp of measurement formatted as CP56Time2a
  sCP56Time2a time;
};
} // namespace Message

} // namespace Remote

#endif // C104_REMOTE_MESSAGE_POINTMESSAGE_H
