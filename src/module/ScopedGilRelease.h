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
 * @file ScopedGilRelease.h
 * @brief conditional py::gil_scoped_release to release GIL only if called from
 * GIL-holding python thread
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_SCOPEDGILRELEASE_H
#define C104_MODULE_SCOPEDGILRELEASE_H

#include <utility>

#include "types.h"

namespace Module {

/**
 * @class ScopedGilRelease
 *
 * @brief The ScopedGilRelease class is used to release the Global Interpreter
 * Lock (GIL) in Python, and re-acquire it when the scope ends.
 */
class ScopedGilRelease {
public:
  // noncopyable
  ScopedGilRelease(const ScopedGilRelease &) = delete;
  ScopedGilRelease &operator=(const ScopedGilRelease &) = delete;

  /**
   * @brief Releasing the GIL within the given scope for non-blocking non-python
   * tasks.
   *
   * If the GIL is not even acquired, it logs the action without releasing.
   *
   * @param callback_name The name of the GIL-dropping scope, used for debugging
   * purposes.
   */
  explicit ScopedGilRelease(std::string callback_name)
      : name(std::move(callback_name)) {
    if (PyGILState_Check()) {
      gil = new py::gil_scoped_release(false);
      DEBUG_PRINT(Debug::Gil, "<--| Release GIL | " + name);
    } else {
      DEBUG_PRINT(Debug::Gil, "?--| (Release) GIL | " + name);
    }
  }

  /**
   * @brief Destructor for the ScopedGilRelease, re-acquiring the Global
   * Interpreter Lock (GIL)
   *
   * The destructor ensures the proper acquiring of the GIL when the
   * ScopedGilRelease object goes out of scope, if and only-if the GIL was
   * released at first place.
   */
  ~ScopedGilRelease() {
    if (gil) {
      delete gil;
      DEBUG_PRINT(Debug::Gil, +"-->| Re-acquire GIL | " + name);
    } else {
      DEBUG_PRINT(Debug::Gil, "--?| (Re-Acquire) GIL | " + name);
    }
  }

private:
  /// @brief name of the GIL-Dropping scope, used for debug logging
  std::string name;

  /// @brief actual GIL reference
  py::gil_scoped_release *gil = nullptr;
};

} // namespace Module

#endif // C104_MODULE_SCOPEDGILRELEASE_H
