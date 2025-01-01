/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file Batch.h
 * @brief class for a collection of outgoing point messages
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_BATCH_H
#define C104_REMOTE_MESSAGE_BATCH_H

#include "object/DataPoint.h"
#include "remote/message/OutgoingMessage.h"

namespace Remote {
namespace Message {

/**
 * @brief model to modify and transmit Remote::Command and Remote::Report
 * objects in a Batch
 */
class Batch : public OutgoingMessage,
              public std::enable_shared_from_this<Batch> {
public:
  // noncopyable
  Batch(const Batch &) = delete;
  Batch &operator=(const Batch &) = delete;

  [[nodiscard]] static std::shared_ptr<Batch>
  create(const CS101_CauseOfTransmission cause,
         const std::optional<Object::DataPointVector> &points = std::nullopt) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Batch>(new Batch(cause, points));
  }

  ~Batch();

  void addPoint(std::shared_ptr<Object::DataPoint> point);

  /**
   * @brief Test if DataPoints exists at this NetworkStation
   * @return information on availability of child DataPoint objects
   */
  bool hasPoints() const;

  /**
   * @brief Get the number of information objects inside this message
   * @return count
   */
  std::uint_fast8_t getNumberOfObjects() const;

  /**
   * @brief Get a list of all DataPoints
   * @return vector with object pointer
   */
  Object::DataPointVector getPoints() const;

  bool isSequence() const override;

  std::string toString() const;

private:
  Batch(CS101_CauseOfTransmission cause,
        const std::optional<Object::DataPointVector> &points);

  /// @brief child DataPoint objects (owned by this Station)
  std::map<std::uint_fast16_t, std::weak_ptr<Object::DataPoint>> weakpts{};

  /// @brief mutex to lock member read/write access
  mutable Module::GilAwareMutex points_mutex{"Batch::points_mutex"};
};

} // namespace Message
} // namespace Remote

#endif // C104_REMOTE_MESSAGE_BATCH_H
