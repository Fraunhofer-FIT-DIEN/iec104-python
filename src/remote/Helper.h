/**
 * Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology
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
 * @file Helper.h
 * @brief formatter and shared functionality related to lib60870-C
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_HELPER_H
#define C104_REMOTE_HELPER_H

// short address not allowed for 104 (only 101) #define
// IEC60870_GLOBAL_COMMON_ADDRESS_SHORT 255
#define IEC60870_GLOBAL_COMMON_ADDRESS 65535

#define IEC60870_FORMAT_OFFSET 2
#define IEC60870_TYPEID_OFFSET 6
#define IEC60870_STRUCTURE_OFFSET 7
#define IEC60870_MSGINFO_OFFSET 8
#define IEC60870_SOURCECA_OFFSET 9
#define IEC60870_TARGETCA1_OFFSET 10
#define IEC60870_TARGETCA2_OFFSET 11
#define IEC60870_OBJECT_OFFSET 12

#include <iec60870_common.h>
#include <iostream>
#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace Remote {
/**
 * @brief test if common address is valid and identifies a single station
 * @param commonAddress address to be tested
 * @return if common address identifies a single station
 */
bool isSingleCommonAddress(std::uint_fast16_t commonAddress);

/**
 * @brief test if common address is a broadcast address
 * @param commonAddress address to be tested
 * @return if common address is a broadcast address
 */
bool isGlobalCommonAddress(std::uint_fast16_t commonAddress);

/**
 * @brief convert a raw message byte stream to a humanly readable string
 * @param msg pointer to first character of message
 * @param msgSize character count of message
 * @return string
 */
std::string rawMessageFormatter(uint_fast8_t *msg, int msgSize);

/**
 * @brief convert a raw message byte stream to a humanly readable string
 * @param msg pointer to first character of message
 * @param msgSize character count of message
 * @return string
 */
py::dict rawMessageDictionaryFormatter(uint_fast8_t *msg, int msgSize);

/**
 * @brief convert a CP56Time2a timestamp to a readable date time string
 * @param time formatted as CP56Time2a
 * @return time formatted as string
 */
std::string CP56Time2a_toString(CP56Time2a time);

/**
 * @brief validate ip and port and join them to a connectionString
 * @param ip ip address or hostname of remote server
 * @param port port address of remote server
 * @return connectionString (ip:port)
 */
std::string connectionStringFormatter(const std::string &ip,
                                      uint_fast16_t port);

/**
 * @brief test if a file exists and is readable
 * @param name file path
 * @return if file is readable or not
 */
bool file_exists(const std::string &name);

} // namespace Remote

#endif // C104_REMOTE_HELPER_H
