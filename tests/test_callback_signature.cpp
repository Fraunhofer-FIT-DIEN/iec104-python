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
 * @file test_callback_signature.cpp
 * @brief exercise the Module::Callback wrapper's signature validation and
 * return-cast logic.
 *
 * @package iec104-python
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include <catch2/catch_test_macros.hpp>
#include <pybind11/eval.h>
#include <pybind11/pybind11.h>

#include <string>
#include <utility>

#include "module/Callback.h"

namespace py = pybind11;

namespace {

// The process-lifetime py::scoped_interpreter is owned by tests/main.cpp, so we
// MUST NOT create a second one here. We only register the python callables once
// into the __main__ namespace and fetch references to them at test time.
const char *kCallbackScript = R"(
import c104

# void callback: exact match for "() -> None"
def _sig_cb_void() -> None:
    pass

# typed callback returning a ResponseState, exact match for "() -> c104.ResponseState"
def _sig_cb_state() -> c104.ResponseState:
    return c104.ResponseState.SUCCESS

# declared return annotation matches, but the runtime value is incompatible
def _sig_cb_state_bad_return() -> c104.ResponseState:
    return "not-a-response-state"

# wrong parameter count -> signature mismatch for "() -> ..."
def _sig_cb_wrong_param_count(value: int) -> None:
    pass

# wrong parameter type -> signature mismatch for "() -> ..."
def _sig_cb_wrong_param_type(value: str) -> None:
    pass
)";

void ensureCallbacks() {
  static bool inited = false;
  if (inited)
    return;
  inited = true;
  py::exec(kCallbackScript);
}

py::object getCallable(const char *name) {
  return py::module_::import("__main__").attr(name);
}

} // namespace

// ---------------------------------------------------------------------------
// Signature validation
// ---------------------------------------------------------------------------

TEST_CASE("Callback<void>: valid signature registers and is set",
          "[callback][signature]") {
  ensureCallbacks();
  // Given: a callback expecting "() -> None" and a python callable matching it
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_void");

  // When: registering the matching callable
  REQUIRE_NOTHROW(cb.reset(callable));

  // Then: the callback is set and callable
  REQUIRE(cb.is_set() == true);
}

TEST_CASE("Callback<void>: invalid parameter count throws and unsets",
          "[callback][signature]") {
  ensureCallbacks();
  // Given: NO parameters expected, but the callable declares one
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_wrong_param_count");

  // When: registering the mismatched callable
  // Then: an invalid_argument is thrown and the callback is left unset
  REQUIRE_THROWS_AS(cb.reset(callable), std::invalid_argument);
  REQUIRE(cb.is_set() == false);
}

TEST_CASE("Callback<void>: invalid parameter type throws and unsets",
          "[callback][signature]") {
  ensureCallbacks();
  // Given: NO parameters expected, but the callable declares a typed parameter
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_wrong_param_type");

  // When: registering the mismatched callable
  // Then: an invalid_argument is thrown and the callback is left unset
  REQUIRE_THROWS_AS(cb.reset(callable), std::invalid_argument);
  REQUIRE(cb.is_set() == false);
}

TEST_CASE("Callback<void>: resetting with None unsets without throwing",
          "[callback][signature]") {
  ensureCallbacks();
  // Given: a callback that is currently set
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_void");
  cb.reset(callable);
  REQUIRE(cb.is_set() == true);

  py::object none = py::none();

  // When: resetting with py::none()
  // Then: no exception is thrown and the callback becomes unset
  REQUIRE_NOTHROW(cb.reset(none));
  REQUIRE(cb.is_set() == false);
}

// ---------------------------------------------------------------------------
// Execution and result retrieval
// ---------------------------------------------------------------------------

namespace {

// Callables that misbehave at call time. Registered separately so the
// signature tests above stay independent of them.
const char *kRaisingScript = R"(
import c104

_sig_cb_raise_count = 0

# raises on every call, signature matches "() -> None"
def _sig_cb_void_raising() -> None:
    global _sig_cb_raise_count
    _sig_cb_raise_count += 1
    raise RuntimeError("boom")

# raises on every call, signature matches "() -> c104.ResponseState"
def _sig_cb_state_raising() -> c104.ResponseState:
    global _sig_cb_raise_count
    _sig_cb_raise_count += 1
    raise KeyError("unknown peer")
)";

void ensureRaisingCallbacks() {
  static bool inited = false;
  if (inited)
    return;
  inited = true;
  py::exec(kRaisingScript);
}

int raiseCount() {
  return py::cast<int>(
      py::module_::import("__main__").attr("_sig_cb_raise_count"));
}

} // namespace

TEST_CASE("Callback<T>: valid call returns the converted result",
          "[callback][execution]") {
  ensureCallbacks();
  // Given: a typed callback and a callable returning a ResponseState
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};
  py::object callable = getCallable("_sig_cb_state");
  cb.reset(callable);
  REQUIRE(cb.is_set() == true);

  // When: invoking the callback
  auto res = cb.call();

  // Then: the converted result is returned
  REQUIRE(res.has_value() == true);
  REQUIRE(res.value() == CommandResponseState::RESPONSE_STATE_SUCCESS);
}

TEST_CASE("Callback<T>: call with no callable set returns nullopt",
          "[callback][execution]") {
  ensureCallbacks();
  // Given: a typed callback that has never been set
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};

  // When: invoking the callback without a callable
  // Then: there is no result
  REQUIRE(cb.call().has_value() == false);
}

// ---------------------------------------------------------------------------
// Return-cast failure path (py::builtin_exception)
// ---------------------------------------------------------------------------

TEST_CASE("Callback<T>: incompatible return value yields no result and keeps "
          "the handler",
          "[callback][execution][return-cast]") {
  ensureCallbacks();
  // Given: a typed callback whose declared annotation matches, but the python
  // callable returns an incompatible value at runtime (a str instead of a
  // ResponseState)
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};
  py::object callable = getCallable("_sig_cb_state_bad_return");
  REQUIRE_NOTHROW(cb.reset(callable));
  REQUIRE(cb.is_set() == true);

  // When: invoking the callback (the return cast fails)
  auto res = cb.call();

  // Then: no result is available, but the handler is NOT removed: a peer
  // must not be able to strip a validation hook by provoking a bad return
  REQUIRE(res.has_value() == false);
  REQUIRE(cb.is_set() == true);

  // And: a second call behaves identically (no fail-open after the first one)
  REQUIRE(cb.call().has_value() == false);
  REQUIRE(cb.is_set() == true);
}

// ---------------------------------------------------------------------------
// Raising callback path (py::error_already_set) — regression for SECURITY H2
// ---------------------------------------------------------------------------

TEST_CASE("Callback<T>: raising callable yields no result and keeps the "
          "handler",
          "[callback][execution][raising]") {
  ensureRaisingCallbacks();
  // Given: a typed callback whose callable raises on every invocation
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};
  py::object callable = getCallable("_sig_cb_state_raising");
  REQUIRE_NOTHROW(cb.reset(callable));
  REQUIRE(cb.is_set() == true);
  int const before = raiseCount();

  // When: invoking the callback twice
  auto first = cb.call();
  auto second = cb.call();

  // Then: both calls report no result, the handler stayed registered and
  // was really invoked both times, and no python error is left pending
  REQUIRE(first.has_value() == false);
  REQUIRE(second.has_value() == false);
  REQUIRE(cb.is_set() == true);
  REQUIRE(raiseCount() == before + 2);
  REQUIRE(PyErr_Occurred() == nullptr);
}

TEST_CASE("Callback<void>: raising callable returns false and keeps the "
          "handler",
          "[callback][execution][raising]") {
  ensureRaisingCallbacks();
  // Given: a void callback whose callable raises on every invocation
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_void_raising");
  REQUIRE_NOTHROW(cb.reset(callable));
  REQUIRE(cb.is_set() == true);
  int const before = raiseCount();

  // When: invoking the callback twice
  bool const first = cb.call();
  bool const second = cb.call();

  // Then: both calls report failure, the handler stayed registered and was
  // invoked both times
  REQUIRE(first == false);
  REQUIRE(second == false);
  REQUIRE(cb.is_set() == true);
  REQUIRE(raiseCount() == before + 2);
  REQUIRE(PyErr_Occurred() == nullptr);
}

TEST_CASE("Callback<void>: valid call returns true", "[callback][execution]") {
  ensureCallbacks();
  // Given: a void callback with a matching callable
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_void");
  cb.reset(callable);

  // When / Then: the call succeeds
  REQUIRE(cb.call() == true);
}

// ---------------------------------------------------------------------------
// sys.unraisablehook integration: the application decides the error policy
// ---------------------------------------------------------------------------

namespace {

// Installs a recording sys.unraisablehook for the lifetime of the object.
struct ScopedUnraisableHook {
  ScopedUnraisableHook() {
    py::exec(R"(
import sys
_sig_hook_calls = []
_sig_hook_prev = sys.unraisablehook
def _sig_hook(unraisable):
    _sig_hook_calls.append((unraisable.exc_type, unraisable.exc_value, unraisable.object))
sys.unraisablehook = _sig_hook
)");
  }
  ~ScopedUnraisableHook() {
    py::exec("import sys\nsys.unraisablehook = _sig_hook_prev");
  }
  py::list calls() const {
    return py::module_::import("__main__").attr("_sig_hook_calls");
  }
};

} // namespace

TEST_CASE("Callback<T>: raising callable is reported via sys.unraisablehook",
          "[callback][execution][raising][unraisablehook]") {
  ensureRaisingCallbacks();
  ScopedUnraisableHook const hook;
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};
  py::object callable = getCallable("_sig_cb_state_raising");
  cb.reset(callable);

  REQUIRE(cb.call().has_value() == false);

  // the hook received the original exception with the callable as context
  auto calls = hook.calls();
  REQUIRE(py::len(calls) == 1);
  auto entry = py::tuple(calls[0]);
  REQUIRE(py::object(entry[0]).is(
      py::reinterpret_borrow<py::object>(PyExc_KeyError)));
  REQUIRE(py::object(entry[2]).is(callable));
  REQUIRE(PyErr_Occurred() == nullptr);
}

TEST_CASE("Callback<T>: incompatible return value is reported as TypeError "
          "via sys.unraisablehook",
          "[callback][execution][return-cast][unraisablehook]") {
  ensureCallbacks();
  ScopedUnraisableHook const hook;
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};
  py::object callable = getCallable("_sig_cb_state_bad_return");
  cb.reset(callable);

  REQUIRE(cb.call().has_value() == false);

  auto calls = hook.calls();
  REQUIRE(py::len(calls) == 1);
  auto entry = py::tuple(calls[0]);
  REQUIRE(py::object(entry[0]).is(
      py::reinterpret_borrow<py::object>(PyExc_TypeError)));
  std::string const msg = py::cast<std::string>(py::str(entry[1]));
  REQUIRE(msg.find("cannot convert returned type") != std::string::npos);
  REQUIRE(msg.find("c104.ResponseState") != std::string::npos);
  REQUIRE(py::object(entry[2]).is(callable));
  REQUIRE(PyErr_Occurred() == nullptr);
}

TEST_CASE("Callback<void>: raising callable is reported via sys.unraisablehook",
          "[callback][execution][raising][unraisablehook]") {
  ensureRaisingCallbacks();
  ScopedUnraisableHook const hook;
  Module::Callback<void> cb{"test.void", "() -> None"};
  py::object callable = getCallable("_sig_cb_void_raising");
  cb.reset(callable);

  REQUIRE(cb.call() == false);

  auto calls = hook.calls();
  REQUIRE(py::len(calls) == 1);
  auto entry = py::tuple(calls[0]);
  REQUIRE(py::object(entry[0]).is(
      py::reinterpret_borrow<py::object>(PyExc_RuntimeError)));
  REQUIRE(py::object(entry[2]).is(callable));
}
