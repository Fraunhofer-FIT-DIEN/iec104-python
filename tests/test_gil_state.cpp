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
 * @file test_gil_state.cpp
 * @brief Regression test for SECURITY.md M15 (SEC-12): GIL ownership decisions
 * must not rely on PyGILState_Check().
 *
 * CPython disables the PyGILState bookkeeping permanently as soon as a second
 * interpreter is created (Py_NewInterpreter() sets gilstate.check_enabled = 0)
 * and PyGILState_Check() then reports "GIL held" for *every* thread, including
 * lib60870 threads that never saw a thread state. Acting on that answer made
 * Module::ScopedGilRelease hand such a thread to PyEval_SaveThread() (and
 * py::gil_scoped_release assert on the same call), which aborts the process.
 *
 * The scenario therefore creates and destroys a subinterpreter through the
 * private _interpreters module, then exercises the module paths that used to
 * consult PyGILState_Check() from a plain std::thread that is never attached:
 * GIL acquisition, a contended GilAwareMutex, a callback invocation and the
 * destruction of a callback that still holds a python reference. All
 * synchronization is event based; the only waits are joins and the pending
 * call drain of the main thread.
 *
 * @package iec104-python
 * @namespace tests
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <atomic>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#ifdef _WIN32
// <windows.h> defines min/max macros that break std::numeric_limits below
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

#include <vector>

#include "module/Callback.h"
#include "module/GilAwareMutex.h"
#include "module/GilState.h"
#include "module/ScopedGilAcquire.h"
#include "module/ScopedGilRelease.h"
#include "types.h"

namespace py = pybind11;

namespace {

/// Name of the test case that performs the scenario in a separate process, see
/// runScenarioInSubprocess().
constexpr const char *kChildCaseName = "GIL state subprocess child";

/// Printed by the child process once the scenario completed without aborting.
constexpr const char *kChildMarker = "GIL_STATE_CHILD_OK";

/// Observations of runSubinterpreterScenario(), all filled in from the test
/// process itself (or from its child, which runs the very same code).
struct ScenarioResult {
  /// PyGILState_Check() as seen by a thread that never had a thread state
  bool foreignSawGilHeldClaim{false};
  /// Module::hasAttachedThreadState() for the same thread
  bool foreignSawAttachedThreadState{true};
  /// the callback could be invoked after the GIL was acquired
  bool callbackInvoked{false};
  /// the foreign thread is detached again after ScopedGilAcquire ended
  bool foreignDetachedAfterAcquire{false};
  /// the contended GilAwareMutex could be locked from the foreign thread
  bool contendedLockAcquired{false};
  /// the foreign thread is detached while it destroys the callback
  bool foreignDetachedForTeardown{false};
  /// the callable handed to the destroyed callback is alive while it owns it
  bool callableAliveWhileOwned{false};
  /// the callable is released after the pending call was drained
  bool callableReleasedAfterDrain{false};
  /// return value of Py_MakePendingCalls()
  int drainResult{-1};
  /// number of python side invocations of the callback
  int callbackHits{0};
};

/// Create and destroy a subinterpreter.
///
/// This is the state M15 occurs in: from here on
/// runtime->gilstate.check_enabled is 0 for the rest of the process lifetime,
/// so PyGILState_Check() answers "GIL held" for unattached threads too, and
/// pybind11 switches to its multi-interpreter internals lookup
/// (has_seen_non_main_interpreter). Only the private _interpreters module is
/// used, no pybind11 object is stored in the new interpreter.
void createAndDestroySubinterpreter() {
  py::object const interpreters = py::module_::import("_interpreters");
  py::object const id = interpreters.attr("create")();
  REQUIRE_FALSE(id.is_none());
  interpreters.attr("destroy")(id);
}

/// @brief true if this interpreter can create a subinterpreter
bool supportsSubinterpreters() {
  try {
    py::module_::import("_interpreters");
    return true;
  } catch (const py::error_already_set &) {
    PyErr_Clear();
    return false;
  }
}

/// Run the M15 scenario: a subinterpreter is created and destroyed, then a
/// never attached std::thread acts like a lib60870 thread.
///
/// Must be called with the GIL held on the main thread of the main
/// interpreter, so the queued reference release can be drained.
ScenarioResult runSubinterpreterScenario() {
  ScenarioResult result;

  createAndDestroySubinterpreter();

  // Fixtures for the two callbacks, registered once per call so they are
  // independent of other tests. The teardown callback is an instance of a
  // callable class, not a function: a weakref to it observes its release
  // without looking at reference counts, which free-threaded builds make
  // unreliable (3.13t reports immortal objects as 0xffffffff).
  py::exec(R"(
import weakref

_gil_state_hits = 0

def _gil_state_on_hit() -> None:
    global _gil_state_hits
    _gil_state_hits += 1

class _GilStateTeardownCallable:
    def __call__(self) -> None:
        pass

_gil_state_teardowns = []

def _gil_state_new_teardown_callable():
    instance = _GilStateTeardownCallable()
    _gil_state_teardowns.append(weakref.ref(instance))
    return instance

def _gil_state_teardown_callable_alive() -> bool:
    return _gil_state_teardowns[-1]() is not None
)",
           py::globals());

  Module::Callback<void> invoked("test_gil_callback", "() -> None");
  py::object onHit = py::globals()["_gil_state_on_hit"];
  invoked.reset(onHit);

  auto torn_down = std::make_unique<Module::Callback<void>>("test_gil_teardown",
                                                            "() -> None");
  {
    // the callback is the only owner of this instance once the scope ends
    py::object onTeardown = py::globals()["_gil_state_new_teardown_callable"]();
    torn_down->reset(onTeardown);
    result.callableAliveWhileOwned =
        py::cast<bool>(py::globals()["_gil_state_teardown_callable_alive"]());
  }

  Module::GilAwareMutex mutex("test_gil_mutex");
  std::atomic_bool holderHasLock{false};
  std::atomic_bool releaseHolder{false};
  std::atomic_bool foreignReachedLock{false};

  {
    // The foreign thread has to acquire the GIL below, so the main thread must
    // not hold it while it waits for the handshakes: release it for the whole
    // thread lifecycle, exactly like waitUntil() in test_teardown.cpp does.
    Module::ScopedGilRelease const whileForeignRuns("test_gil_main_wait");

    // plain std::thread without a thread state, like the lib60870 threads
    std::thread holder([&]() {
      mutex.lock();
      holderHasLock.store(true);
      while (!releaseHolder.load()) {
        std::this_thread::yield();
      }
      mutex.unlock();
    });
    while (!holderHasLock.load()) {
      std::this_thread::yield();
    }

    std::thread foreign([&]() {
      // the poisoned answer that M15 used to act on
      result.foreignSawGilHeldClaim = PyGILState_Check() != 0;
      result.foreignSawAttachedThreadState = Module::hasAttachedThreadState();

      {
        // must acquire instead of assuming the GIL is held
        Module::ScopedGilAcquire const gil("test_gil_foreign_acquire");
        result.callbackInvoked = invoked.call();
      }
      result.foreignDetachedAfterAcquire = !Module::hasAttachedThreadState();

      {
        // must not release a GIL this thread never held
        Module::ScopedGilRelease const release("test_gil_foreign_release");
      }

      // the holder still owns the mutex here, so the slow path runs: it used to
      // release "the GIL" of a thread without a thread state
      foreignReachedLock.store(true);
      mutex.lock();
      result.contendedLockAcquired = true;
      mutex.unlock();

      // destroying a live callback on a foreign thread must not touch the
      // reference directly: it is handed to the interpreter
      result.foreignDetachedForTeardown = !Module::hasAttachedThreadState();
      torn_down.reset();
    });

    while (!foreignReachedLock.load()) {
      std::this_thread::yield();
    }
    releaseHolder.store(true);
    holder.join();
    foreign.join();
  }

  py::object none = py::none();
  invoked.reset(none);
  result.callbackHits = py::cast<int>(py::globals()["_gil_state_hits"]);

  // deterministic drain instead of waiting for the next eval breaker check
  result.drainResult = Py_MakePendingCalls();
  result.callableReleasedAfterDrain =
      !py::cast<bool>(py::globals()["_gil_state_teardown_callable_alive"]());
  return result;
}

/// @brief path of the running test executable, empty if it cannot be resolved
std::string currentExecutablePath() {
#ifdef _WIN32
  std::wstring buffer(4096, L'\0');
  DWORD const length = ::GetModuleFileNameW(nullptr, buffer.data(),
                                            static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return {};
  }
  buffer.resize(length);
  std::u8string const utf8 = std::filesystem::path(buffer).u8string();
  return std::string(utf8.begin(), utf8.end());
#elif defined(__linux__)
  std::vector<char> buffer(4096, '\0');
  ssize_t const length =
      ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
  if (length <= 0) {
    return {};
  }
  return std::string(buffer.data(), static_cast<size_t>(length));
#else
  return {};
#endif
}

/// Run the scenario in a fresh test process and report its exit code.
///
/// The subinterpreter disables the PyGILState bookkeeping for the rest of the
/// process lifetime, which is exactly what the issue asks to survive, but it
/// must not leak into the tests that share this process. The child runs only
/// the kChildCaseName test case.
py::object runScenarioInSubprocess(const std::string &executable) {
  auto subprocess = py::module_::import("subprocess");
  return subprocess.attr("run")(py::make_tuple(executable, kChildCaseName),
                                "capture_output"_a = true, "text"_a = true,
                                "timeout"_a = 180);
}

} // namespace

// ===========================================================================
// M15 / SEC-12: GIL ownership decisions
// ===========================================================================

TEST_CASE("PyGILState_Check lies about unattached threads after a "
          "subinterpreter existed",
          "[module][gil][security]") {
  if (!supportsSubinterpreters()) {
    SKIP("this interpreter cannot create subinterpreters");
  }

  ScenarioResult const result = runSubinterpreterScenario();

  // the condition M15 is about: CPython reports "GIL held" for a thread that
  // never had a thread state
  REQUIRE(result.foreignSawGilHeldClaim);
  // and the supported check answers correctly
  REQUIRE_FALSE(result.foreignSawAttachedThreadState);

  REQUIRE(result.callbackInvoked);
  REQUIRE(result.callbackHits == 1);
  REQUIRE(result.foreignDetachedAfterAcquire);
  REQUIRE(result.contendedLockAcquired);
  REQUIRE(result.foreignDetachedForTeardown);

  // the queued release ran: the reference the callback held is gone
  REQUIRE(result.callableAliveWhileOwned);
  REQUIRE(result.drainResult == 0);
  REQUIRE(result.callableReleasedAfterDrain);
}

TEST_CASE("callbacks and mutexes survive a subinterpreter in a fresh process",
          "[module][gil][security]") {
  if (!supportsSubinterpreters()) {
    SKIP("this interpreter cannot create subinterpreters");
  }
  std::string const executable = currentExecutablePath();
  if (executable.empty()) {
    SKIP("cannot resolve the path of this test executable");
  }

  py::object const completed = runScenarioInSubprocess(executable);
  int const returncode = py::cast<int>(completed.attr("returncode"));
  std::string const output = py::cast<std::string>(completed.attr("stdout")) +
                             py::cast<std::string>(completed.attr("stderr"));

  INFO("child output: " << output);
  REQUIRE(returncode == 0);
  REQUIRE(output.find(kChildMarker) != std::string::npos);
}

TEST_CASE(kChildCaseName, "[gil-subprocess-child][.]") {
  if (!supportsSubinterpreters()) {
    SKIP("this interpreter cannot create subinterpreters");
  }

  ScenarioResult const result = runSubinterpreterScenario();

  REQUIRE(result.foreignSawGilHeldClaim);
  REQUIRE_FALSE(result.foreignSawAttachedThreadState);
  REQUIRE(result.callbackInvoked);
  REQUIRE(result.contendedLockAcquired);
  REQUIRE(result.callableAliveWhileOwned);
  REQUIRE(result.drainResult == 0);
  REQUIRE(result.callableReleasedAfterDrain);

  std::cout << kChildMarker << std::endl;
}
