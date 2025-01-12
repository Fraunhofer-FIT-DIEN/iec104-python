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

  /**
   * @brief Creates a new Batch object with a specified cause of transmission
   * and optional data points.
   *
   * This method dynamically allocates memory for a Batch object using its
   * private constructor. It ensures proper initialization of the Batch instance
   * with the provided parameters.
   *
   * @param cause The cause of transmission associated with the new Batch.
   * @param points An optional collection of DataPoints to be added to the Batch
   * during creation. If provided, the points are processed and verified for
   * compatibility with the Batch. Defaults to std::nullopt, indicating no
   * initial points.
   * @return A shared pointer to the newly created Batch object.
   * @throws std::invalid_argument if a point in points is not a monitoring
   * point, if it lacks a station reference, if it is already in the Batch, or
   * if its type or station is incompatible with the Batch.
   */
  [[nodiscard]] static std::shared_ptr<Batch>
  create(const CS101_CauseOfTransmission cause,
         const std::optional<Object::DataPointVector> &points = std::nullopt) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Batch>(new Batch(cause, points));
  }

  /**
   * @brief clearing the map of DataPoints
   */
  ~Batch() override;

  /**
   * @brief Adds a DataPoint to the Batch while ensuring compatibility and
   * preventing duplicates.
   *
   * This method checks if the provided DataPoint meets specific criteria for
   * type and station compatibility before adding it to the Batch. It also
   * ensures that the DataPoint is not already present in the Batch.
   *
   * @param point A shared pointer to the DataPoint object to be added.
   *        Only monitoring points are allowed, and the type and station
   *        must be consistent with those already in the Batch.
   * @throws std::invalid_argument if the point is not a monitoring point,
   *         if it lacks a station reference, if it is already in the Batch,
   *         or if its type or station is incompatible with the Batch.
   */
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

  /**
   * @brief Checks if the keys of DataPoints in the Batch are sequential.
   *
   * Determines if the keys of the stored DataPoints form a continuous,
   * ordered sequence without gaps, skipping expired entries.
   *
   * @return True if the keys are sequential or if the collection is empty,
   *         otherwise false.
   */
  bool isSequence() const override;

  /**
   * @brief Converts the Batch object to a string representation.
   *
   * Generates a string that provides detailed information about the Batch
   * object, including its common address, type, cause of transmission, number
   * of objects, and various status flags such as test, negative, and sequence.
   * Also includes the memory location of the object.
   *
   * @return A string representation of the Batch object.
   */
  std::string toString() const;

private:
  /**
   * @brief Constructs a Batch object with a specified cause of transmission and
   * optional data points.
   *
   * Initializes the Batch with the provided cause, and if data points are
   * given, they are added to the Batch after being verified for compatibility.
   *
   * @param cause The cause of transmission associated with this Batch.
   * @param points An optional collection of DataPoints to be added to the
   * Batch. If provided, the points are processed and added during construction.
   * @throws std::invalid_argument if a point in points is not a monitoring
   * point, if it lacks a station reference, if it is already in the Batch, or
   * if its type or station is incompatible with the Batch.
   */
  Batch(CS101_CauseOfTransmission cause,
        const std::optional<Object::DataPointVector> &points);

  /// @brief contained DataPoint objects (non-owned)
  std::map<std::uint_fast16_t, std::weak_ptr<Object::DataPoint>> pointMap{};

  /// @brief mutex to lock member read/write access
  mutable Module::GilAwareMutex points_mutex{"Batch::points_mutex"};
};

} // namespace Message
} // namespace Remote

#endif // C104_REMOTE_MESSAGE_BATCH_H
