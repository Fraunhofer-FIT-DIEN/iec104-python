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
 * @file Station.cpp
 * @brief 60870-5-104 station
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "object/Station.h"
#include "Server.h"
#include "remote/Connection.h"

using namespace Object;

Station::Station(std::uint_fast16_t st_commonAddress,
                 std::shared_ptr<Server> st_server,
                 std::shared_ptr<Remote::Connection> st_connection)
    : commonAddress(st_commonAddress), server(st_server),
      connection(st_connection) {
  DEBUG_PRINT(Debug::Station, "Created");
}

Station::~Station() {
  {
    std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
    points.clear();
  }
  DEBUG_PRINT(Debug::Station, "Removed");
}

std::uint_fast16_t Station::getCommonAddress() const { return commonAddress; }

std::shared_ptr<Server> Station::getServer() {
  if (server.expired()) {
    return {nullptr};
  }
  return server.lock();
}

std::shared_ptr<Remote::Connection> Station::getConnection() {
  if (connection.expired()) {
    return {nullptr};
  }
  return connection.lock();
}

bool Station::hasPoints() const {
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);

  return !points.empty();
}

DataPointVector Station::getPoints() const {
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);

  return points;
}

std::shared_ptr<DataPoint>
Station::getPoint(const std::uint_fast32_t informationObjectAddress) {
  if (0 == informationObjectAddress) {
    return {nullptr};
  }

  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  for (auto &p : points) {
    if (p->getInformationObjectAddress() == informationObjectAddress) {
      return p;
    }
  }
  return {nullptr};
}

std::shared_ptr<DataPoint>
Station::addPoint(const std::uint_fast32_t informationObjectAddress,
                  const IEC60870_5_TypeID type,
                  const std::uint_fast32_t reportInterval_ms,
                  const std::uint_fast32_t relatedInformationObjectAddress,
                  const bool relatedInformationObjectAutoReturn,
                  const CommandTransmissionMode commandMode) {
  if (getPoint(informationObjectAddress)) {
    return {nullptr};
  }

  DEBUG_PRINT(Debug::Station,
              "add_point] " + std::string(TypeID_toString(type)) + " | IOA " +
                  std::to_string(informationObjectAddress));

  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  auto point =
      DataPoint::create(informationObjectAddress, type, shared_from_this(),
                        reportInterval_ms, relatedInformationObjectAddress,
                        relatedInformationObjectAutoReturn, commandMode);
  // auto point = new DataPoint(informationObjectAddress, type, this,
  // reportInterval_ms, relatedInformationObjectAddress,
  // relatedInformationObjectAutoReturn);

  points.push_back(point);
  return point;
}

bool Station::isLocal() { return !server.expired(); }
