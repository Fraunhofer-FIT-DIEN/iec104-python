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
 * @file Executor.h
 * @brief task executor thread manager
 *
 * @package iec104-python
 * @namespace Tasks
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_TASKS_EXECUTOR_H
#define C104_TASKS_EXECUTOR_H

#include "tasks/Task.h"
#include <atomic>
#include <condition_variable>
#include <functional>
#include <queue>
#include <thread>

namespace Tasks {

class Executor : public std::enable_shared_from_this<Executor> {
protected:
  /// @brief server thread to execute periodic transmission
  std::thread *runThread = nullptr;

  /// @brief conditional variable to stop server thread without waiting for
  /// another loop
  mutable std::condition_variable_any runThread_wait{};

  /// @brief server thread mutex to not lock thread execution
  mutable std::mutex queue_mutex{};

  std::priority_queue<Task> queue;

  /// @brief state that defines if server thread should be running
  std::atomic_bool enabled{true};

  /// @brief server thread state
  std::atomic_bool running{false};

  void thread_run();

public:
  // noncopyable
  Executor(const Executor &) = delete;
  Executor &operator=(const Executor &) = delete;

  Executor();
  ~Executor();
  void stop();

  /**
   * @brief Schedules a periodic task to be executed at a specified interval.
   *
   * @param task A callable object representing the task to be executed
   * periodically.
   * @param interval The interval in milliseconds at which the task should be
   * executed. Must be at least 50ms. Throws std::out_of_range if the interval
   * is less than 50ms.
   */
  void addPeriodic(const std::function<void()> &task,
                   std::int_fast32_t interval);

  /**
   * @brief Schedules a task to be executed after a specified delay (or
   * instant).
   *
   * The order of execution will depend on the timestamp calculated from current
   * time + delay. The delay may be negative for high priority tasks.
   *
   * @param task The function to be executed.
   * @param delay The delay in milliseconds before the task is executed. A
   * negative delay executes the task immediately.
   */
  void add(const std::function<void()> &task, std::int_fast32_t delay = 0);
};

} // namespace Tasks

#endif // C104_TASKS_EXECUTOR_H
