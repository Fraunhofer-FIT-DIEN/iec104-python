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
 * @file Callback.h
 * @brief mangage python callbacks and verify function signatures
 *
 * @package iec104-python
 * @namespace module
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_MODULE_CALLBACK_H
#define C104_MODULE_CALLBACK_H

#include <optional>
#include <utility>

#include "module/GilAwareMutex.h"
#include "module/ScopedGilAcquire.h"
#include "types.h"

using namespace pybind11::literals;

namespace Module {

/**
 * @brief The CallbackBase class.
 *
 * This class represents a base class for callback functions.
 * A callback function is a function that is passed as an argument to another
 * function. This class provides functionality for setting, resetting, and
 * checking if the callback function is set.
 */
class CallbackBase {
public:
  CallbackBase(std::string cb_name, std::string cb_signature)
      : callback(py::none()), name(std::move(cb_name)) {
    cb_signature.erase(
        remove_if(cb_signature.begin(), cb_signature.end(), isspace),
        cb_signature.end());
    signature = std::move(cb_signature);
  }

  // noncopyable: copies would duplicate the python reference without the GIL
  CallbackBase(const CallbackBase &) = delete;
  CallbackBase &operator=(const CallbackBase &) = delete;

  /**
   * @brief Releases the python reference with the GIL held.
   *
   * The owning object may be destroyed on a lib60870 or worker thread that
   * dropped the last shared_ptr, so the GIL cannot be assumed here.
   */
  ~CallbackBase() {
#if PY_VERSION_HEX >= 0x030D0000
    bool const finalizing = Py_IsFinalizing();
#else
    bool const finalizing = _Py_IsFinalizing();
#endif
    if (!Py_IsInitialized() || finalizing) {
      // interpreter is gone or going: leak instead of touching refcounts
      callback.release();
      return;
    }
    ScopedGilAcquire const scoped(name + ".destroy");
    callback = py::object();
  }

  /**
   * @brief Resets the callback function to a new value.
   *
   * This function generates a signature for the `callable` object, and compares
   *it with the expected signature. If the signatures don't match, the callback
   *function is unset, and an `invalid_argument ` exception is thrown.
   *
   * @param callable The new callback function.
   *
   * @throws std::invalid_argument If the `callable` object is not a callable or
   *if its signature does not match the expected signature.
   */
  void reset(py::object &callable) {
    DEBUG_PRINT(Debug::Callback, "REGISTER " + name);

    if (callable.is_none()) {
      unset();
      return;
    }

    auto inspect = py::module_::import("inspect");
    auto empty = inspect.attr("Parameter").attr("empty");

    // throws if callback is not a callable
    auto sig = inspect.attr("signature")(callable);
    // create a derived signature object without non-empty parameters
    auto parameters = py::dict(sig.attr("parameters"));
    auto empty_params = py::list();
    for (auto param : parameters) {
      if (param.second.attr("default").is(empty)) {
        empty_params.append(param.second);
      }
    }
    auto sig1 = inspect.attr("Signature")("parameters"_a = empty_params,
                                          "return_annotation"_a =
                                              sig.attr("return_annotation"));
    std::string callable_signature = py::cast<std::string>(py::str(sig1));
    callable_signature.erase(remove_if(callable_signature.begin(),
                                       callable_signature.end(), isspace),
                             callable_signature.end());

    if (signature != callable_signature) {
      unset();
      throw std::invalid_argument("Invalid callback signature, expected: " +
                                  signature + ", got: " + callable_signature);
    }
    {
      std::lock_guard<Module::GilAwareMutex> const lock(callback_mutex);
      callback = callable;
    }
  }

  /**
   * @brief Check if the callback function is set.
   *
   * This function checks if the callback object is not None.
   *
   * @return true if the callback function is set, false otherwise.
   */
  bool is_set() const {
    std::lock_guard<Module::GilAwareMutex> const lock(callback_mutex);

    return !callback.is_none();
  }

protected:
  /**
   * @brief Unsets the callback function.
   *
   * This function unsets the callback function by setting it to `py::none()`.
   */
  void unset() noexcept(true) {
    DEBUG_PRINT(Debug::Callback, "CLEAR " + name);
    std::lock_guard<Module::GilAwareMutex> const lock(callback_mutex);
    if (!callback.is_none()) {
      callback = py::none();
    }
  }

  /**
   * @brief Take a snapshot of the callback reference.
   *
   * @return the callable, or py::none() if no callback is set
   */
  py::object snapshot() const {
    std::lock_guard<Module::GilAwareMutex> const lock(callback_mutex);
    return callback;
  }

  /**
   * @brief Report an exception raised by the python callback.
   *
   * The handler stays registered: a remote peer must not be able to strip a
   * validation hook by provoking an exception in it (callers treat a failed
   * call as a rejected request instead). The exception is handed to
   * sys.unraisablehook with the callable as context, exactly like python does
   * for exceptions in __del__ or ctypes callbacks: the default hook prints
   * the traceback to stderr, applications may install their own hook to
   * alert, count or terminate. Must not throw, because call() is noexcept and
   * runs on lib60870 threads.
   *
   * @param cb the callable that raised
   * @param e the exception fetched by pybind11
   */
  void reportException(const py::object &cb, py::error_already_set &e) const
      noexcept(true) {
    DEBUG_PRINT(Debug::Callback,
                name + "] raised, request rejected, handler kept");
    try {
      e.discard_as_unraisable(cb);
    } catch (...) {
      // the hook itself failed; python already dropped the error
    }
  }

  /**
   * @brief Report a return value that cannot be converted to the expected type.
   *
   * Raises a TypeError through sys.unraisablehook, see reportException() for
   * the no-throw and keep-handler rationale.
   *
   * @param cb the callable that produced the value
   * @param result_type the python type name of the returned value
   */
  void reportCastError(const py::object &cb,
                       const std::string &result_type) const noexcept(true) {
    DEBUG_PRINT(Debug::Callback, name + "] incompatible return value, "
                                        "request rejected, handler kept");
    try {
      std::string expected = signature.substr(signature.find("->") + 2);
      try {
        auto inspect = py::module_::import("inspect");
        auto sig = inspect.attr("signature")(cb);
        expected =
            py::cast<std::string>(py::str(sig.attr("return_annotation")));
      } catch (...) {
        // fall back to the annotation of the registered signature
      }
      std::string const msg =
          name + ": cannot convert returned type " + result_type +
          " to expected type " + expected +
          ". Please make sure your callback function is returning an "
          "instance of " +
          expected + " or a compatible type.";
      PyErr_SetString(PyExc_TypeError, msg.c_str());
      PyErr_WriteUnraisable(cb.ptr());
    } catch (...) {
      PyErr_Clear();
    }
  }

  /// @brief callback function reference
  py::object callback;

  /// @brief callback function name, used for debug logging
  std::string name{"Callback"};

  /// @brief callback function signature, used for validation
  std::string signature{"() -> None"};

  /// @brief mutex for accessing the callback object
  mutable Module::GilAwareMutex callback_mutex{"Callback::callback_mutex"};
};

/**
 * @brief The Callback class (with return type).
 *
 * This class is used for managing user-defined callback functions.
 * It allows assigning a function with well-known signature to be executed at a
 * specific point, and provides mechanisms for invoking the function securely
 * and receiving the return value.
 */
template <typename T> class Callback : public CallbackBase {
public:
  Callback(std::string cb_name, std::string cb_signature)
      : CallbackBase(std::move(cb_name), std::move(cb_signature)) {}

  /**
   * @brief Calls the callback function with the given values.
   *
   * The result is returned by value instead of being stored in the object, so
   * concurrent calls (free-threaded builds) can never observe each other's
   * result. A callback that raises or returns an incompatible value stays
   * registered; the caller decides what a missing result means (fail closed).
   *
   * @tparam Types The types of the values to pass to the callback
   * @param values The values to pass to the callback
   * @return the converted return value, or std::nullopt if no callback is set,
   * the callback raised or the return value could not be converted to T
   */
  template <typename... Types>
  std::optional<T> call(Types &&...values) noexcept(true) {
    auto const cb = this->snapshot();
    if (cb.is_none()) {
      return std::nullopt;
    }

    // locals, not members: call() may run concurrently in free-threaded builds
    std::chrono::steady_clock::time_point begin;
    if (DEBUG_TEST(Debug::Callback)) {
      begin = std::chrono::steady_clock::now();
    }

    std::optional<T> result;
    std::string result_type{"None"};
    try {
      py::object res = cb(std::forward<Types>(values)...);

      // Safely get the result type as a string
      result_type = py::cast<std::string>(py::str(res.get_type()));

      // Cast the result to the expected type
      result = res.cast<T>();
    } catch (py::error_already_set &e) {
      this->reportException(cb, e);
    } catch (py::builtin_exception &e) {
      this->reportCastError(cb, result_type);
    }

    if (DEBUG_TEST(Debug::Callback)) {
      DEBUG_PRINT_CONDITION(true, Debug::Callback,
                            name + "] Stats | TOTAL " + TICTOCNOW(begin));
    }

    return result;
  }
};

/**
 * @brief The Callback class (no return type).
 *
 * This class is used for managing user-defined callback functions.
 * It allows assigning a function with well-known signature to be executed at a
 * specific point, and provides mechanisms for invoking the function securely
 * without receiving a return value.
 */
template <> class Callback<void> : public CallbackBase {
public:
  Callback(const std::string &cb_name, const std::string &cb_signature)
      : CallbackBase(cb_name, cb_signature) {}

  /**
   * @brief Calls the callback function with the given values.
   *
   * A callback that raises stays registered.
   *
   * @tparam Types The types of the values.
   * @param values The values to pass to the callback function.
   * @return bool Returns true if the callback function is called successfully,
   * false if no callback is set or the callback raised.
   */
  template <typename... Types> bool call(Types &&...values) noexcept(true) {
    auto const cb = this->snapshot();
    if (cb.is_none()) {
      return false;
    }

    // locals, not members: call() may run concurrently in free-threaded builds
    std::chrono::steady_clock::time_point begin;
    if (DEBUG_TEST(Debug::Callback)) {
      begin = std::chrono::steady_clock::now();
    }

    bool success = false;
    try {
      cb(std::forward<Types>(values)...);
      success = true;
    } catch (py::error_already_set &e) {
      this->reportException(cb, e);
    }

    if (DEBUG_TEST(Debug::Callback)) {
      DEBUG_PRINT_CONDITION(true, Debug::Callback,
                            name + "] Stats | TOTAL " + TICTOCNOW(begin));
    }

    return success;
  }
};

} // namespace Module

#endif // C104_MODULE_CALLBACK_H
