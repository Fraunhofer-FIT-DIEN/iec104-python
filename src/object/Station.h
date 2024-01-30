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

  [[nodiscard]] static std::shared_ptr<Station>
  create(std::uint_fast16_t st_commonAddress,
         std::shared_ptr<Server> st_server = nullptr,
         std::shared_ptr<Remote::Connection> st_connection = nullptr) {
    // Not using std::make_shared because the constructor is private.
    return std::shared_ptr<Station>(new Station(
        st_commonAddress, std::move(st_server), std::move(st_connection)));
  }

  /**
   * @brief Remove object
   */
  ~Station();

  // @todo import/export station with DataPoints to string

private:
  explicit Station(std::uint_fast16_t st_commonAddress,
                   std::shared_ptr<Server> st_server,
                   std::shared_ptr<Remote::Connection> st_connection);

  /// @brief unique common address of this station
  std::uint_fast16_t commonAddress{0};

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
  std::uint_fast16_t getCommonAddress() const;

  std::shared_ptr<Server> getServer();

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
   * @param relatedInformationObjectAddress related information object address
   * @param relatedInformationObjectAutoReturn auto transmit related point on
   * command
   * @param commandMode command transmission mode (direct or select-and-execute)
   * @throws std::invalid_argument if type is invalid
   */
  std::shared_ptr<DataPoint>
  addPoint(std::uint_fast32_t informationObjectAddress, IEC60870_5_TypeID type,
           std::uint_fast32_t reportInterval_ms = 0,
           std::uint_fast32_t relatedInformationObjectAddress = 0,
           bool relatedInformationObjectAutoReturn = false,
           CommandTransmissionMode commandMode = DIRECT_COMMAND);

  bool isLocal();

public:
  inline friend std::ostream &operator<<(std::ostream &os, Station &s) {
    os << std::endl
       << "+------------------------------+" << '\n'
       << "| DUMP Asset/Station           |" << '\n'
       << "+------------------------------+" << '\n'
       << "|" << std::setw(19) << "ASDU/CA: " << std::setw(10)
       << std::to_string(s.commonAddress) << " |" << '\n'
       << "|------------------------------+" << std::endl;
    return os;
  }
};

/**
 * @brief vector definition of Station objects
 */
typedef std::vector<std::shared_ptr<Station>> StationVector;
} // namespace Object

#endif // C104_OBJECT_STATION_H
