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
 * @file Tuple.h
 * @brief convert c++ lists to immutable python tuples
 *
 * @package iec104-python
 * @namespace Module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_TUPLE_H
#define C104_MODULE_TUPLE_H

#include <pybind11/pybind11.h>

namespace py = pybind11;

namespace Module {

// Convert list to tuple (immutable)
template <typename T>
inline py::tuple vector_to_tuple(const std::vector<T> &data) {
  py::tuple result(data.size());
  size_t i = 0;
  for (const auto &item : data) {
    result[i++] = item;
  }
  return result;
}

} // namespace Module

#endif // C104_MODULE_TUPLE_H
