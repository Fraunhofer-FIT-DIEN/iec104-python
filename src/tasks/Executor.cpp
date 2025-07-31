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
 * @file Executor.cpp
 * @brief task executor thread manager
 *
 * @package iec104-python
 * @namespace Tasks
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "Executor.h"
#include "debug.h"
#include <iostream>

using namespace Tasks;

Executor::Executor() {
  DEBUG_PRINT_NAMED(Debug::Server | Debug::Client, "Executor",
                    "Task executor created");
  runThread = new std::thread(&Executor::thread_run, this);
}

Executor::~Executor() {
  stop();
  DEBUG_PRINT_NAMED(Debug::Server | Debug::Client, "Executor",
                    "Task executor destroyed");
}

void Executor::stop() {
  enabled.store(false);
  runThread_wait.notify_all();

  if (runThread) {
    runThread->join();
    delete runThread;
    runThread = nullptr;
  }
}

void Executor::thread_run() {
  bool const debug = DEBUG_TEST(Debug::Server | Debug::Client);
  running.store(true);
  while (enabled.load()) {
    std::function<void()> task;

    {
      std::unique_lock<std::mutex> lock(queue_mutex);
      if (queue.empty()) {
        runThread_wait.wait(lock);
        continue;
      }
      const auto top = queue.top();

      auto now = std::chrono::steady_clock::now();
      if (now >= top.schedule_time) {
        auto delay = now - top.schedule_time;
        if (delay > DELAY_THRESHOLD) {
          DEBUG_PRINT_CONDITION(
              debug, "Executor",
              "Warning: Task started delayed by " +
                  std::to_string(
                      std::chrono::duration_cast<std::chrono::milliseconds>(
                          delay)
                          .count()) +
                  " ms");
        }
        task = top.function;
        queue.pop();
      } else {
        runThread_wait.wait_until(lock, top.schedule_time);
        continue;
      }
    }

    try {
      task();
    } catch (const std::exception &e) {
      std::cerr << "[c104.Executor] loop] Task aborted: " << e.what()
                << std::endl;
    }
  }
  {
    std::unique_lock<std::mutex> lock(queue_mutex);
    if (!queue.empty()) {
      DEBUG_PRINT_CONDITION(debug, "Executor",
                            "loop] Tasks dropped due to stop: " +
                                std::to_string(queue.size()));
      std::priority_queue<Task> empty;
      std::swap(queue, empty);
    }
  }
  running.store(false);
}

void Executor::add(const std::function<void()> &task,
                   const std::int_fast32_t delay) {
  {
    if (!enabled.load())
      std::cerr << "[c104.Executor] add] Task dropped due to stop" << std::endl;

    std::lock_guard<std::mutex> lock(queue_mutex);
    if (delay < 0) {
      queue.push({task, std::chrono::steady_clock::time_point::min()});
    } else {
      queue.push({task, std::chrono::steady_clock::now() +
                            std::chrono::milliseconds(delay)});
    }
  }

  runThread_wait.notify_one();
}

void Executor::addPeriodic(const std::function<void()> &task,
                           const std::int_fast32_t interval) {
  if (!enabled.load())
    std::cerr << "[c104.Executor] add] Task dropped due to stop" << std::endl;

  if (interval < 50) {
    throw std::out_of_range(
        "The interval for periodic tasks must be 50ms at minimum.");
  }

  std::weak_ptr<Executor> weakSelf =
      shared_from_this(); // Weak reference to `this`

  auto periodicCallback = [weakSelf, task, interval]() {
    auto self = weakSelf.lock();
    if (!self)
      return; // Prevent running if `this` was destroyed

    // Schedule next execution
    self->add(task, interval); // Reschedule itself

    task();
  };

  // Schedule first execution
  add(periodicCallback, interval);
}
