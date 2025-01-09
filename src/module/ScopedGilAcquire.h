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
 * @file ScopedGilAcquire.h
 * @brief conditional py::gil_scoped_acquire to acquire GIL only if called from
 * Non-GIL-Holding python thread
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_SCOPEDGILCALLBACK_H
#define C104_MODULE_SCOPEDGILCALLBACK_H

#include <utility>

#include "types.h"

namespace Module {

/**
 * @class ScopedGilAcquire
 * @brief The ScopedGilAcquire class is a utility class that provides a scoped
 * acquisition and release of the Global Interpreter Lock (GIL).
 *
 * The ScopedGilAcquire is used to safely acquire and release the GIL within a
 * specific scope, ensuring that the Python interpreter is protected from
 * concurrent access by multiple threads.
 */
class ScopedGilAcquire {
public:
  // noncopyable
  ScopedGilAcquire(const ScopedGilAcquire &) = delete;
  ScopedGilAcquire &operator=(const ScopedGilAcquire &) = delete;

  /**
   * @brief Acquiring the GIL within the given scope for safe interaction with
   * the Python interpreter.
   *
   * If the GIL is already acquired, it logs the action without re-acquiring.
   *
   * @param callback_name A string representing the name of the scope or
   * function acquiring the GIL, used for debugging and logging purposes.
   */
  explicit ScopedGilAcquire(std::string callback_name)
      : gil(), name(std::move(callback_name)) {
    if (PyGILState_Check()) {
      DEBUG_PRINT(Debug::Gil, "--?| (Acquire) GIL | " + name);
    } else {
      DEBUG_PRINT(Debug::Gil, "-->| Acquire GIL | " + name);
      gil = new py::gil_scoped_acquire();
    }
  }

  /**
   * @brief Destructor for the ScopedGilAcquire class, releasing the Global
   * Interpreter Lock (GIL).
   *
   * The destructor ensures the proper release of the GIL when the
   * ScopedGilAcquire object goes out of scope, if and only-if the GIL was
   * acquired at first place.
   */
  ~ScopedGilAcquire() {
    if (gil) {
      delete gil;
      DEBUG_PRINT(Debug::Gil, "<--| Re-release GIL | " + name);
    } else {
      DEBUG_PRINT(Debug::Gil, "?--| (Release) GIL | " + name);
    }
  }

private:
  /// @brief name of the GIL-Holding scope, used for debug logging
  std::string name;

  /// @brief actual GIL reference
  py::gil_scoped_acquire *gil = nullptr;
};
}; // namespace Module

#endif // C104_MODULE_SCOPEDGILCALLBACK_H
