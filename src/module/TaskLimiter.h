/**
 * Copyright 2026-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file TaskLimiter.h
 * @brief bound the number of queued tasks that are fed by peer frames
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_TASKLIMITER_H
#define C104_MODULE_TASKLIMITER_H

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <utility>

namespace Module {

/**
 * @brief Bounds the number of pending notification tasks of one run loop
 *
 * @details Every frame received or sent can schedule a task that calls a
 * Python callback (on_receive_raw, on_send_raw, on_unexpected_message). If
 * the callback is slower than the frame rate, the run loop's task queue would
 * otherwise grow without bound (see SECURITY.md L7). Server and Client each
 * own one limiter with MAX_PENDING_NOTIFICATION_TASKS slots; connections use
 * the limiter of their client.
 *
 * A slot is represented by a Token. acquire() is the admission check: it
 * returns nullptr if all slots are taken, then the notification must be
 * dropped. The token's *lifetime* is the accounting: the slot stays occupied
 * as long as any copy of the token exists, so the token has to be captured by
 * the scheduled task even though the task body never uses it. The slot is
 * then released when the last copy of the task (std::function) is destroyed,
 * i.e. after the run loop executed it, or when the queue is discarded on
 * stop. Without the capture the slot would be released at the end of the
 * scheduling function and the limit would never be reached.
 *
 * @code
 * auto token = acquireNotificationToken("on_receive_raw");
 * if (!token) {
 *   return; // limit reached, drop this notification
 * }
 * // capture token: it holds the slot until the task is destroyed
 * scheduleTask([weakSelf, data, token]() { ... });
 * @endcode
 *
 * Drops are counted (dropped()) and reported to stderr at most once every
 * WARNING_INTERVAL_MS. Tokens share the internal state, so they may outlive
 * the limiter (e.g. a task still queued while its owner is destroyed).
 * All methods are thread-safe.
 */
class TaskLimiter {
public:
  /// @brief keeps one slot occupied for as long as it is alive
  using Token = std::shared_ptr<void>;

  /**
   * @param owner name used in the warning message, e.g. "c104.Server"
   * @param limit maximum number of pending tasks holding a token
   */
  TaskLimiter(std::string owner, std::size_t limit)
      : state(std::make_shared<State>(std::move(owner), limit)) {}

  TaskLimiter(const TaskLimiter &) = delete;
  TaskLimiter &operator=(const TaskLimiter &) = delete;

  /**
   * @brief reserve a slot for a new notification task
   * @param what name of the dropped notification, used in the warning
   * @return a non-null token to capture in the task, or nullptr if the limit
   * is reached and the notification has to be dropped
   */
  Token acquire(const char *what) {
    auto s = state;
    std::size_t current = s->pending.load();
    do {
      if (current >= s->limit) {
        s->drop(what);
        return nullptr;
      }
    } while (!s->pending.compare_exchange_weak(current, current + 1));
    // aliasing a non-null pointer, so callers can test the token for null
    return {s.get(), [s](void *) { s->pending.fetch_sub(1); }};
  }

  /// @brief number of tasks currently holding a token
  [[nodiscard]] std::size_t pending() const { return state->pending.load(); }

  /// @brief number of notifications dropped since creation
  [[nodiscard]] std::uint64_t dropped() const { return state->dropped.load(); }

  /// @brief maximum number of pending tasks
  [[nodiscard]] std::size_t limit() const { return state->limit; }

private:
  struct State {
    State(std::string o, const std::size_t l) : owner(std::move(o)), limit(l) {}

    void drop(const char *what) {
      dropped.fetch_add(1);
      const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::steady_clock::now().time_since_epoch())
                           .count();
      auto last = lastWarning.load();
      if (last != 0 && now - last < WARNING_INTERVAL_MS) {
        return;
      }
      if (!lastWarning.compare_exchange_strong(last, now)) {
        return; // another thread prints the warning
      }
      std::cerr << "[" << owner << "] Python callback queue full (" << limit
                << " pending), dropping " << what
                << " notifications. Total dropped: " << dropped.load()
                << std::endl;
    }

    static constexpr std::int64_t WARNING_INTERVAL_MS = 10000;

    const std::string owner;
    const std::size_t limit;
    std::atomic_size_t pending{0};
    std::atomic_uint64_t dropped{0};
    std::atomic<std::int64_t> lastWarning{0};
  };

  /// @brief shared with every token, so tokens may outlive the limiter
  std::shared_ptr<State> state;
};

/// @brief pending Python notification tasks per Server or Client
constexpr std::size_t MAX_PENDING_NOTIFICATION_TASKS = 10000;

} // namespace Module

#endif // C104_MODULE_TASKLIMITER_H
