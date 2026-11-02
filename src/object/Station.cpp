/**
 * Copyright 2020-2026 Fraunhofer Institute for Applied Information Technology
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
#include "Client.h"
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
    pointIoaMap.clear();
    points.clear();
  }
  DEBUG_PRINT(Debug::Station, "Removed");
}

std::uint_fast16_t Station::getCommonAddress() const { return commonAddress; }

std::shared_ptr<Server> Station::getServer() {
  std::lock_guard<std::mutex> const lock(parent_mutex);
  return server.lock();
}

std::shared_ptr<Remote::Connection> Station::getConnection() {
  std::lock_guard<std::mutex> const lock(parent_mutex);
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
  // IOA 0 is a valid address (DataPoint accepts 0..MAX_IOA)
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  auto const it = pointIoaMap.find(informationObjectAddress);
  if (it == pointIoaMap.end()) {
    return {nullptr};
  }
  return it->second;
}

std::shared_ptr<DataPoint> Station::addPoint(
    const std::uint_fast32_t informationObjectAddress,
    const IEC60870_5_TypeID type, const std::uint_fast16_t reportInterval_ms,
    const std::optional<std::uint_fast32_t> relatedInformationObjectAddress,
    const bool relatedInformationObjectAutoReturn,
    const CommandTransmissionMode commandMode) {
  // forward tickRate_ms
  uint_fast16_t tickRate_ms = 0;
  if (auto sv = getServer()) {
    tickRate_ms = sv->getTickRate_ms();
  } else if (auto co = getConnection()) {
    if (auto cl = co->getClient()) {
      tickRate_ms = cl->getTickRate_ms();
    }
  }
  // Server and Client enforce tickRate_ms >= 50, so 0 means the owner is gone.
  // Reject here, so a DataPoint always has a valid tick rate (report interval
  // and timer checks divide by it).
  if (tickRate_ms == 0) {
    throw std::runtime_error("Cannot add a point to a station whose server or "
                             "client no longer exists");
  }

  // dedup and insert under the same lock, so two threads adding the same IOA
  // concurrently cannot both succeed
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
  if (pointIoaMap.count(informationObjectAddress) > 0) {
    return {nullptr};
  }

  DEBUG_PRINT(Debug::Station,
              "add_point] " + std::string(TypeID_toString(type)) + " | IOA " +
                  std::to_string(informationObjectAddress));

  auto point = DataPoint::create(
      informationObjectAddress, type, shared_from_this(), reportInterval_ms,
      relatedInformationObjectAddress, relatedInformationObjectAutoReturn,
      commandMode, tickRate_ms);

  points.push_back(point);
  pointIoaMap.emplace(informationObjectAddress, point);
  return point;
}

bool Station::removePoint(const std::uint_fast32_t informationObjectAddress) {
  std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);

  DEBUG_PRINT(Debug::Station,
              "remove_point] IOA " + std::to_string(informationObjectAddress));

  auto const it = pointIoaMap.find(informationObjectAddress);
  if (it == pointIoaMap.end()) {
    return false;
  }

  auto const point = it->second;
  pointIoaMap.erase(it);
  points.erase(std::remove(points.begin(), points.end(), point), points.end());
  point->detach();

  return true;
}

bool Station::isLocal() {
  std::lock_guard<std::mutex> const lock(parent_mutex);
  return !server.expired();
}

void Station::sendEndOfInitialization(const CS101_CauseOfInitialization cause) {
  if (auto sv = getServer()) {
    return sv->sendEndOfInitialization(commonAddress, cause);
  }

  throw std::runtime_error(
      "Cannot send end of initialization: not a server station");
}

void Station::detach() {
  std::lock_guard<std::mutex> const lock(parent_mutex);
  server.reset();
  connection.reset();
}
