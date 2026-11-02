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
 * @file GilAwareMutex.h
 * @brief mutex that is aware of pythons Global Interpreter Lock to avoid
 * deadlocks with GIL
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Lennart Bader <lennart.bader@fkie.fraunhofer.de>, Martin Unkel
 * <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_GILAWAREMUTEX_H
#define C104_MODULE_GILAWAREMUTEX_H

#include <iostream>
#include <mutex>
#include <utility>

#include "module/ScopedGilRelease.h"

using namespace std::chrono_literals;

namespace Module {
/**
 * @class GilAwareMutex
 *
 * @brief A mutex class that handles the GIL (Global Interpreter Lock)
 * automatically.
 *
 * The GilAwareMutex class provides a mutex that automatically releases and
 * re-acquires the GIL when locking. This is useful in
 * multi-threaded applications that interact with the Python interpreter, as it
 * allows other Python threads to continue executing while the lock is held.
 */
class GilAwareMutex {
public:
  // Create a new combined mutex that automatically handles the GIL
  inline GilAwareMutex() = default;

  inline explicit GilAwareMutex(std::string _name) : name(std::move(_name)) {}

  /**
   * @brief Locks the GilAwareMutex.
   *
   * An uncontended lock is taken without touching the GIL. Otherwise the GIL
   * is released while waiting, so a holder that needs the GIL to finish its
   * critical section can make progress: no thread ever waits for this mutex
   * while holding the GIL, which makes "mutex, then GIL" a consistent lock
   * order. The GIL is re-acquired before returning, callers see their GIL
   * state unchanged.
   *
   * This call blocks until the lock is available. It used to throw
   * std::runtime_error after 100 ms, but lib60870 invokes our handlers from C
   * code and several callers are noexcept, so the exception terminated the
   * process; a remote peer could provoke that by timing a request while a
   * python thread held a mutex. A wait beyond 100 ms is reported on stderr
   * (once per call) so that a genuine lock order deadlock stays diagnosable.
   */
  inline void lock() {
    if (this->wrapped_mutex.try_lock()) {
      return;
    }
    ScopedGilRelease const scoped_gil(name + "::lock_gil_aware");
    if (this->wrapped_mutex.try_lock_for(100ms)) {
      return;
    }
    std::cerr << "[c104] Potential Deadlock: mutex " << name
              << " waiting for lock > 100ms" << std::endl;
    this->wrapped_mutex.lock();
  }

  /**
   * @brief Unlocks the GilAwareMutex.
   *
   * This function unlocks the GilAwareMutex by releasing the lock.
   *
   * @note This function should only be called after acquiring the lock using
   * the `lock()` function.
   */
  inline void unlock() { this->wrapped_mutex.unlock(); }

  /**
   * @brief Tries to lock the GilAwareMutex.
   *
   * Never blocks, so the GIL is left untouched.
   *
   * @return True if the lock was acquired successfully, false otherwise.
   */
  inline bool try_lock() { return this->wrapped_mutex.try_lock(); }

private:
  /// @brief name of the mutex, used for debug logging
  std::string name{"GilAwareMutex"};

  /// @brief actual mutex
  std::timed_mutex wrapped_mutex{};
};
}; // namespace Module

#endif // C104_MODULE_GILAWAREMUTEX_H
