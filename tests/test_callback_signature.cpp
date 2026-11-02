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

TEST_CASE("Callback<T>: valid call returns true and getResult returns T",
          "[callback][execution]") {
  ensureCallbacks();
  // Given: a typed callback and a callable returning a ResponseState
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};
  py::object callable = getCallable("_sig_cb_state");
  cb.reset(callable);
  REQUIRE(cb.is_set() == true);

  // When: invoking the callback
  bool ok = cb.call();

  // Then: the call succeeded and the expected result is available
  REQUIRE(ok == true);
  REQUIRE(cb.getResult() == CommandResponseState::RESPONSE_STATE_SUCCESS);
}

TEST_CASE("Callback<T>: call with no callable set returns false",
          "[callback][execution]") {
  ensureCallbacks();
  // Given: a typed callback that has never been set
  Module::Callback<CommandResponseState> cb{"test.state",
                                            "() -> c104.ResponseState"};

  // When: invoking the callback without a callable
  // Then: the call reports failure
  REQUIRE(cb.call() == false);
}

// ---------------------------------------------------------------------------
// Return-cast failure path (py::builtin_exception)
// ---------------------------------------------------------------------------

TEST_CASE("Callback<T>: incompatible return value unsets and loses the result",
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
  bool ok = cb.call();

  // Then: the call reports failure, the callback is auto-unset, and no result
  // is available
  REQUIRE(ok == false);
  REQUIRE(cb.is_set() == false);
  REQUIRE_THROWS_AS(cb.getResult(), std::invalid_argument);
  try {
    cb.getResult();
    FAIL("getResult() after a failed return cast must throw");
  } catch (const std::invalid_argument &e) {
    REQUIRE(std::string(e.what()) == "No result set!");
  }
}
