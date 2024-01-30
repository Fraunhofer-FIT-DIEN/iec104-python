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
 * re-acquires the GIL when locking and unlocking. This is useful in
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
   * This function acquires the GilAwareMutex lock. If the lock is not available
   * within 100 milliseconds, it throws a std::runtime_error with a potential
   * deadlock message.
   *
   * @throws std::runtime_error If the GilAwareMutex is unable to acquire the
   * lock within 100ms.
   */
  inline void lock() {
    ScopedGilRelease const scoped_gil(name + "::lock_gil_aware");
    if (!this->wrapped_mutex.try_lock_for(100ms)) {
      throw std::runtime_error("Potential Deadlock: mutex " + name +
                               " waiting for lock > 100ms");
    }
  }

  /**
   * @brief Unlocks the GilAwareMutex.
   *
   * This function unlocks the GilAwareMutex by releasing the lock. It ensures
   * that the GIL (Global Interpreter Lock) is re-acquired after unlocking. This
   * is useful in multi-threaded applications that interact with the Python
   * interpreter, as it allows other Python threads to continue executing while
   * the lock is released.
   *
   * @note This function should only be called after acquiring the lock using
   * the `lock()` function.
   */
  inline void unlock() {
    ScopedGilRelease const scoped_gil(name + "::unlock_gil_aware");
    this->wrapped_mutex.unlock();
  }

  /**
   * @brief Tries to lock the GilAwareMutex.
   *
   * This function attempts to acquire the GilAwareMutex lock. If the lock is
   * available, it will be acquired and the function will return true. If the
   * lock is not available, the function will return false.
   *
   * @return True if the lock was acquired successfully, false otherwise.
   */
  inline bool try_lock() {
    ScopedGilRelease const scoped_gil(name + "::try_lock_gil_aware");
    return this->wrapped_mutex.try_lock();
  }

private:
  std::string name{"GilAwareMutex"};
  std::timed_mutex wrapped_mutex{};
};
}; // namespace Module

#endif // C104_MODULE_GILAWAREMUTEX_H
