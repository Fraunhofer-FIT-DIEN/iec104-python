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
 * @file Station.h
 * @brief 60870-5-104 station
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_STATION_H
#define C104_OBJECT_STATION_H

#include "DataPoint.h"
#include "module/GilAwareMutex.h"
#include "types.h"

namespace Object {

class Station : public std::enable_shared_from_this<Station> {
public:
  // noncopyable
  Station(const Station &) = delete;
  Station &operator=(const Station &) = delete;

  /**
   * @brief Creates a new Station instance with the specified parameters.
   *
   * This static method creates a Station object with the provided common
   * address and optional server or remote connection references. The
   * constructor is private, so this method must be used to instantiate a
   * Station.
   *
   * @param st_commonAddress The unique common address of the station.
   * @param st_server A shared pointer to the owning server, if applicable.
   * Default is nullptr.
   * @param st_connection A shared pointer to the remote connection, if
   * applicable. Default is nullptr.
   * @return A shared pointer to the newly created Station instance.
   */
  [[nodiscard]] static std::shared_ptr<Station>
  create(std::uint_fast16_t st_commonAddress,
         std::shared_ptr<Server> st_server = nullptr,
         std::shared_ptr<Remote::Connection> st_connection = nullptr) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Station>(new Station(
        st_commonAddress, std::move(st_server), std::move(st_connection)));
  }

  /**
   * @brief Remove station and cleanup all related DataPoints
   */
  ~Station();

  // @todo import/export station with DataPoints to string

private:
  explicit Station(std::uint_fast16_t st_commonAddress,
                   std::shared_ptr<Server> st_server,
                   std::shared_ptr<Remote::Connection> st_connection);

  /// @brief unique common address of this station
  const std::uint_fast16_t commonAddress{0};

  /// @brief server object reference (only local station)
  std::weak_ptr<Server> server;

  /// @brief remote connection object reference (only remote station)
  std::weak_ptr<Remote::Connection> connection;

  /// @brief child DataPoint objects (owned by this Station)
  DataPointVector points{};

  /// @brief mutex to lock member read/write access
  mutable Module::GilAwareMutex points_mutex{"Station::points_mutex"};

  /// @brief conversion hashmap {IOA,not owning pointer to child DataPoints} to
  /// find a DataPoint via IOA
  std::unordered_map<std::uint_fast32_t, std::shared_ptr<DataPoint>>
      pointIoaMap{};

public:
  /**
   * @brief getter for commonAddress
   * @return unique common address of this station
   */
  std::uint_fast16_t getCommonAddress() const;

  /**
   * @brief getter for server
   * @return shared pointer to the owning server instance, optional
   */
  std::shared_ptr<Server> getServer();

  /**
   * @brief getter for connection
   * @return shared pointer to the owning connection instance, optional
   */
  std::shared_ptr<Remote::Connection> getConnection();

  /**
   * @brief Test if DataPoints exists at this NetworkStation
   * @return information on availability of child DataPoint objects
   */
  bool hasPoints() const;

  /**
   * @brief Get a list of all DataPoints
   * @return vector with object pointer
   */
  DataPointVector getPoints() const;

  /**
   * @brief Get a DataPoint that exists at this NetworkStation and is identified
   * via information object address
   * @return Pointer to DataPoint or nullptr
   */
  std::shared_ptr<DataPoint>
  getPoint(std::uint_fast32_t informationObjectAddress);

  /**
   * @brief Add a DataPoint to this Station
   * @param informationObjectAddress information object address
   * @param type iec60870-5-104 information type
   * @param reportInterval_ms auto reporting interval
   * @param relatedInformationObjectAddress related information object address,
   * if any
   * @param relatedInformationObjectAutoReturn auto transmit related point on
   * command
   * @param commandMode command transmission mode (direct or select-and-execute)
   * @throws std::invalid_argument if type is invalid
   */
  std::shared_ptr<DataPoint>
  addPoint(std::uint_fast32_t informationObjectAddress, IEC60870_5_TypeID type,
           std::uint_fast16_t reportInterval_ms = 0,
           std::optional<std::uint_fast32_t> relatedInformationObjectAddress =
               std::nullopt,
           bool relatedInformationObjectAutoReturn = false,
           CommandTransmissionMode commandMode = DIRECT_COMMAND);

  /**
   * @brief Removes an existing DataPoint from this Station.
   * @param informationObjectAddress The address of the information object to be
   * removed.
   * @return True if the DataPoint was successfully found and removed, otherwise
   * false.
   */
  bool removePoint(std::uint_fast32_t informationObjectAddress);

  /**
   * @brief test if this station belongs to a server instance and not a
   * connection (client)
   * @return if it has a server
   */
  bool isLocal();

  /**
   * @brief Sends the end of initialization signal with the specified cause.
   * @param cause The reason for the initialization end, as defined in
   * CS101_CauseOfInitialization.
   * @throws std::runtime_error if the station is not a server.
   */
  void sendEndOfInitialization(CS101_CauseOfInitialization cause);

  /**
   * @brief Remove reference to station, do not call this method, this is called
   * by Station::removePoint
   */
  void detach();

  /**
   * @brief Generates a string representation of the Station object.
   *
   * This method provides a detailed description of the Station object,
   * including its common address, the number of data points, and its memory
   * address.
   *
   * @return A string representing the Station object.
   */
  std::string toString() const {
    size_t len = 0;
    {
      std::scoped_lock<Module::GilAwareMutex> const lock(points_mutex);
      len = points.size();
    }
    std::ostringstream oss;
    oss << "<104.Station common_address=" << std::to_string(commonAddress)
        << ", #points=" << std::to_string(len) << " at " << std::hex
        << std::showbase << reinterpret_cast<std::uintptr_t>(this) << ">";
    return oss.str();
  };
};

/**
 * @brief vector definition of Station objects
 */
typedef std::vector<std::shared_ptr<Station>> StationVector;
} // namespace Object

#endif // C104_OBJECT_STATION_H
