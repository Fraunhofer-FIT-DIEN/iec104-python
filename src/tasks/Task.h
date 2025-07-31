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
 * @file Task.h
 * @brief task structure definition
 *
 * @package iec104-python
 * @namespace Tasks
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_TASKS_TASK_H
#define C104_TASKS_TASK_H

#include "constants.h"
#include <chrono>

// Capture helper: accepts any number of captures via __VA_ARGS__
#define SAFE_LAMBDA_CAPTURE(body, ...)                                         \
  [weakSelf, __VA_ARGS__]() {                                                  \
    auto self = weakSelf.lock();                                               \
    if (!self)                                                                 \
      return;                                                                  \
    body                                                                       \
  }

// No captures
#define SAFE_LAMBDA(body)                                                      \
  [weakSelf]() {                                                               \
    auto self = weakSelf.lock();                                               \
    if (!self)                                                                 \
      return;                                                                  \
    body                                                                       \
  }

// Simple function call
#define SAFE_TASK_CAPTURE(fn, ...)                                             \
  SAFE_LAMBDA_CAPTURE({ self->fn(__VA_ARGS__); }, __VA_ARGS__)
#define SAFE_TASK(fn) SAFE_LAMBDA({ self->fn(); })

namespace Tasks {

/**
 * @brief Represents a task with a name, description, and completion status.
 *
 * Provides functionality to manage and query the task's state, including
 * marking it as complete.
 */
struct Task {
  std::function<void()> function;
  std::chrono::steady_clock::time_point schedule_time;
  bool operator<(const Task &rhs) const {
    return schedule_time > rhs.schedule_time;
  }
};

/**
 * @brief Defines the threshold duration for task delay warnings.
 * @details This constant is used to compare against the actual delay of a
 * scheduled task execution. If the delay exceeds this threshold,
 * a warning message will be logged indicating that the task execution
 * was delayed beyond the acceptable duration.
 */
constexpr auto DELAY_THRESHOLD =
    std::chrono::milliseconds(TASK_DELAY_THRESHOLD_MS);

} // namespace Tasks

#endif // C104_TASKS_TASK_H
