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
 * @file ThreadOwner.h
 * @brief mark threads on which their owning object must not be torn down
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_THREADOWNER_H
#define C104_MODULE_THREADOWNER_H

#include <atomic>
#include <cstdint>

namespace Module {

/**
 * @brief Generate a process-wide unique key for lib60870 callback parameters.
 *
 * Server and Connection register themselves under this key for their static
 * lib60870 handlers. Using the object address instead would let a lib60870
 * handle whose teardown is still in progress on a neutral thread (see
 * ScopedThreadOwner) deliver late callbacks to a new instance that happens to
 * be allocated at the same address.
 *
 * @return a non-null pointer-sized token that is never handed out twice
 */
inline void *nextCallbackKey() noexcept {
  static std::atomic<std::uintptr_t> counter{1};
  return reinterpret_cast<void *>(
      counter.fetch_add(1, std::memory_order_relaxed));
}

/**
 * @class ScopedThreadOwner
 *
 * @brief Marks the current thread as running on behalf of an object whose
 * teardown would have to join this very thread.
 *
 * lib60870 handlers and the run loops of Server and Client hold a shared_ptr
 * to their instance while they execute, and python callbacks receive a strong
 * reference as well. If python drops its last reference in the meantime
 * (typically from inside a callback, or by interpreter exit), one of these
 * references becomes the last owner and the destructor runs on the lib60870
 * thread or on the run thread itself. Joining the current thread
 * (CS104_Slave_stop, CS104_Connection_close, std::thread::join) is either a
 * self-deadlock, an exception from a destructor or frees memory the current
 * stack still uses. Destructors call isCurrentThreadOwnedBy() to detect this
 * and hand the blocking part of their teardown to a neutral thread instead.
 *
 * A marker must be created before the shared_ptr it protects, otherwise it
 * is already gone when that shared_ptr releases the object at the end of
 * the scope. lib60870 handlers therefore mark the callback key (their
 * parameter, see nextCallbackKey()) before resolving the instance, run
 * loops mark their owner for the lifetime of the thread. Markers nest, are
 * strictly scoped and allocation free.
 *
 * A thread that runs on behalf of an object it cannot name - lib60870 raises
 * TLS security events directly on the handshake or connection thread, which
 * carries no callback key - marks ANY instead, which matches every owner in
 * isCurrentThreadOwnedBy(). That lets a python callback invoked from such a
 * thread drop the last reference to a Server, Client, Connection or
 * TransportSecurity without tearing it down under the running callback.
 */
class ScopedThreadOwner {
public:
  /**
   * @brief Owner key that protects every object about to be torn down.
   *
   * Pass this as owner_ptr when the current thread runs on behalf of an object
   * whose key is unknowable, for example a lib60870 TLS event handler that is
   * called from the handshake thread. isCurrentThreadOwnedBy() reports a match
   * for such a marker whatever key the destructor asks about.
   *
   * The token is a pointer-sized value that no allocation can ever have, so it
   * never collides with a real owner. It is a marker key only: it is not an
   * object address for markDestroyed() and ownerDestroyed() stays false on a
   * wildcard marker.
   */
  inline static const void *const ANY =
      reinterpret_cast<const void *>(static_cast<std::uintptr_t>(-1));

  /**
   * @brief Mark the current thread as owned by owner_ptr until destruction.
   *
   * @param owner_ptr the object whose teardown must not run on this thread,
   * or ANY to protect every object; nullptr is accepted and never matches
   */
  explicit ScopedThreadOwner(const void *owner_ptr) noexcept
      : owner(owner_ptr), prev(top()) {
    top() = this;
  }

  ~ScopedThreadOwner() { top() = prev; }

  // noncopyable: the marker is bound to the scope it was created in
  ScopedThreadOwner(const ScopedThreadOwner &) = delete;
  ScopedThreadOwner &operator=(const ScopedThreadOwner &) = delete;

  /**
   * @brief Check if a marker for owner_ptr is active on the calling thread.
   *
   * A marker created with ANY matches every owner_ptr, so an owner destroyed
   * on a thread that cannot name its own key still defers its teardown.
   *
   * @param owner_ptr the object about to be torn down
   * @return true if the calling thread executes on behalf of owner_ptr
   */
  static bool isCurrentThreadOwnedBy(const void *owner_ptr) noexcept {
    if (!owner_ptr) {
      return false;
    }
    for (auto *marker = top(); marker; marker = marker->prev) {
      if (marker->owner == owner_ptr || marker->owner == ANY) {
        return true;
      }
    }
    return false;
  }

  /**
   * @brief Flag every active marker of owner_ptr on the calling thread.
   *
   * Called by the owner's destructor when it runs on its own run thread, so
   * the loop behind the marker returns without touching the destroyed object,
   * see ownerDestroyed().
   *
   * Only keyed markers are flagged: a wildcard marker (ANY) does not describe a
   * run loop of a single object and is never reported as destroyed.
   *
   * @param owner_ptr the object being destroyed
   */
  static void markDestroyed(const void *owner_ptr) noexcept {
    for (auto *marker = top(); marker; marker = marker->prev) {
      if (marker->owner == owner_ptr) {
        marker->destroyed = true;
      }
    }
  }

  /**
   * @brief Check if the owner of this marker was destroyed on this thread.
   *
   * @return true if markDestroyed() was called for the owner of this marker
   */
  [[nodiscard]] bool ownerDestroyed() const noexcept { return destroyed; }

private:
  /// @brief innermost active marker of the calling thread
  static ScopedThreadOwner *&top() noexcept {
    static thread_local ScopedThreadOwner *current = nullptr;
    return current;
  }

  /// @brief the object this thread executes on behalf of
  const void *owner;

  /// @brief enclosing marker on this thread, nullptr for the outermost one
  ScopedThreadOwner *prev;

  /// @brief set by markDestroyed()
  bool destroyed{false};
};

} // namespace Module

#endif // C104_MODULE_THREADOWNER_H
