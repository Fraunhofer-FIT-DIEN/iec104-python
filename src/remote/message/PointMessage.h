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
 * @file PointMessage.h
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
  /**
   * @brief Creates a new instance of PointMessage for a given DataPoint.
   *
   * This method creates a shared pointer to a PointMessage object corresponding
   * to the specified DataPoint.
   *
   * @param point The DataPoint object for which the message is created. This
   * represents the value that should be reported to a remote client.
   * @return A shared pointer to a newly created PointMessage instance.
   * @throws std::invalid_argument if point reference, point type, station
   * reference is invalid
   */
  [[nodiscard]] static std::shared_ptr<PointMessage>
  create(std::shared_ptr<Object::DataPoint> point) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<PointMessage>(new PointMessage(std::move(point)));
  }

  /**
   * @brief free information object
   */
  ~PointMessage() override;

private:
  /**
   * @brief Create a message for a certain DataPoint, type of message is
   * identified via DataPoint
   * @param point point whose value should be reported to remote client
   * @throws std::invalid_argument if point reference, point type, station
   * reference is invalid
   */
  explicit PointMessage(std::shared_ptr<Object::DataPoint> point);
};
} // namespace Message

} // namespace Remote

#endif // C104_REMOTE_MESSAGE_POINTMESSAGE_H
