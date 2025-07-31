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
 * @file constants.h
 * @brief shared constant definitions
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_CONSTANTS_H
#define C104_CONSTANTS_H

// short address not allowed for 104 (only 101) #define
// IEC60870_GLOBAL_COMMON_ADDRESS_SHORT 255
#define IEC60870_GLOBAL_COMMON_ADDRESS 65535

#define MAX_INFORMATION_OBJECT_ADDRESS 16777215

#define UNDEFINED_INFORMATION_OBJECT_ADDRESS 16777216

#define TASK_DELAY_THRESHOLD_MS 100

// max number of QOI groups
#define NUM_GROUPS 16

#endif // C104_CONSTANTS_H
