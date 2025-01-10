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
 * @file Batch.cpp
 * @brief class for a collection of outgoing point messages
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Batch.h"
#include "object/Station.h"

using namespace Remote::Message;

Batch::Batch(const CS101_CauseOfTransmission cause,
             const std::optional<Object::DataPointVector> &points)
    : OutgoingMessage() {
  causeOfTransmission.store(cause);

  DEBUG_PRINT(Debug::Message,
              "Batch Created " +
                  std::string(CS101_CauseOfTransmission_toString(cause)) +
                  " at " +
                  std::to_string(reinterpret_cast<std::uintptr_t>(this)));

  if (points.has_value()) {
    for (auto point : points.value()) {
      addPoint(point);
    }
  }
};

Batch::~Batch() {
  pointMap.clear();
  DEBUG_PRINT(Debug::Message,
              "Batch Removed " +
                  std::string(CS101_CauseOfTransmission_toString(
                      causeOfTransmission.load())) +
                  " at " +
                  std::to_string(reinterpret_cast<std::uintptr_t>(this)));
}

void Batch::addPoint(std::shared_ptr<Object::DataPoint> point) {

  // only monitoring points
  if (point->getType() > 41)
    throw std::invalid_argument(
        "Only monitoring points are allowed in a batch");

  auto _station = point->getStation();
  if (!_station) {
    throw std::invalid_argument("Cannot get station from point");
  }

  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  if (pointMap.empty()) {
    type = point->getType();
    commonAddress = _station->getCommonAddress();
  } else {
    // already added?
    auto it = pointMap.find(point->getInformationObjectAddress());
    // Already exists, throw or handle as needed
    if (it != pointMap.end() && !it->second.expired()) {
      throw std::invalid_argument("Point already added to batch");
    }

    // test compatibility
    if (type != point->getType()) {
      throw std::invalid_argument("Incompatible types in batch");
    }
    if (commonAddress != _station->getCommonAddress()) {
      throw std::invalid_argument("Incompatible stations in batch");
    }
  }

  pointMap[point->getInformationObjectAddress()] = point;
  DEBUG_PRINT(Debug::Message,
              "Point added to batch " +
                  std::to_string(point->getInformationObjectAddress()));
}

bool Batch::hasPoints() const {
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);

  return !pointMap.empty();
}

std::uint_fast8_t Batch::getNumberOfObjects() const {
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  return pointMap.size();
}

Object::DataPointVector Batch::getPoints() const {
  Object::DataPointVector points;
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);

  for (auto it = pointMap.begin(); it != pointMap.end(); ++it) {
    // Remove expired pointers
    if (it->second.expired())
      continue;
    points.push_back(it->second.lock());
  }

  return points;
}

bool Batch::isSequence() const {
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  if (pointMap.size() < 2) {
    return true; // cannot be a sequence without at least two elements
  }

  // Iterator-based traversal to test key sequences
  // std::map is always ordered, therefore sorting is not necessary
  auto it = pointMap.begin();
  while (it->second.expired() && it != pointMap.end()) {
    ++it;
  }
  if (it == pointMap.end()) {
    return true;
  }

  uint_fast16_t previousKey = it->first;
  ++it;

  for (; it != pointMap.end(); ++it) {
    if (it->second.expired())
      continue;
    if (it->first != previousKey + 1) {
      return false; // Keys are not sequential
    }
    previousKey = it->first;
  }

  return true; // All keys are sequential
}

std::string Batch::toString() const {
  std::ostringstream oss;
  oss << "<c104.Batch common_address=" << std::to_string(commonAddress)
      << ", type=" << TypeID_toString(type)
      << ", cot=" << CS101_CauseOfTransmission_toString(causeOfTransmission)
      << ", number_of_objects=" << std::to_string(getNumberOfObjects())
      << ", test=" << bool_toString(test)
      << ", negative=" << bool_toString(negative)
      << ", sequence=" << bool_toString(isSequence()) << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
  return oss.str();
};
