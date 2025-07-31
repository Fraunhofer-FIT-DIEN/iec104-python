/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied IInformation Technology
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
 * @file Information.h
 * @brief protocol specific helper function for information objects
 *
 * @package iec104-python
 * @namespace Transformer
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_TRANSFORMER_INFORMATION_H
#define C104_TRANSFORMER_INFORMATION_H

#include "iec60870_common.h"
#include <cstdint>
#include <memory>

namespace Object {
namespace Information {
class IInformation;
}
} // namespace Object

namespace Transformer {

/**
 * @brief create an information instance from protocol message
 * @param io iec60870 information object
 * @return information instance
 * @throws std::invalid_argument if type not supported
 */
std::shared_ptr<Object::Information::IInformation>
fromInformationObject(InformationObject io);

/**
 * @brief get protocol message from information instance
 * @param info information instance
 * @param timestamp wether to return a iec60870 message with or without
 * timestamp
 * @return iec60870 information object
 * @throws std::invalid_argument if desired timestamp option not supported
 */
InformationObject
asInformationObject(std::shared_ptr<Object::Information::IInformation> info,
                    std::uint_fast32_t informationObjectAddress,
                    bool timestamp);

} // namespace Transformer
#endif // C104_TRANSFORMER_INFORMATION_H
