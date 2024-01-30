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

#include <utility>

#include "module/GilAwareMutex.h"
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
  void unset() {
    DEBUG_PRINT(Debug::Callback, "CLEAR " + name);
    {
      std::lock_guard<Module::GilAwareMutex> const lock(callback_mutex);
      if (!callback.is_none()) {
        callback = py::none();
      }
    }
    success = false;
  }

  py::object callback;

  std::string name{"Callback"};
  std::string signature{"() -> None"};

  std::atomic_bool success{false};

  std::chrono::steady_clock::time_point begin, end;

  mutable Module::GilAwareMutex callback_mutex{"Callback::callback_mutex"};
};

template <typename T> class Callback : public CallbackBase {
public:
  Callback(std::string cb_name, std::string cb_signature)
      : CallbackBase(std::move(cb_name), std::move(cb_signature)) {}

  /**
   * @brief Calls the callback function with the given values.
   *
   * @tparam Types The types of the values to pass to the callback
   * @param values The values to pass to the callback
   * @return bool True if the callback was called successfully, false otherwise
   */
  template <typename... Types> bool call(Types &&...values) {
    std::unique_lock<Module::GilAwareMutex> lock(this->callback_mutex);
    auto const cb = this->callback;
    lock.unlock();

    if (cb.is_none()) {
      return false;
    }

    if (DEBUG_TEST(Debug::Callback)) {
      this->begin = std::chrono::steady_clock::now();
    }

    try {
      py::object res = cb(std::forward<Types>(values)...);
      result = py::cast<T>(res);
      this->success = true;
    } catch (py::error_already_set &e) {
      this->success = false;
      std::cerr
          << '\n'
          << "------------------------------------------------------------"
          << '\n'
          << '\n'
          << this->name << "] Error:" << std::endl;

      auto traceback = py::module_::import("traceback");
      traceback.attr("print_exception")(e.type(), e.value(), e.trace());

      std::cerr
          << "------------------------------------------------------------"
          << '\n'
          << std::endl;
      this->unset();
    }

    if (DEBUG_TEST(Debug::Callback)) {
      this->end = std::chrono::steady_clock::now();
      DEBUG_PRINT_CONDITION(
          true, Debug::Callback,
          name + "] Stats | TOTAL " +
              std::to_string(
                  std::chrono::duration_cast<std::chrono::microseconds>(
                      this->end - this->begin)
                      .count()) +
              u8" \xb5s");
    }

    return this->success;
  }

  /**
   * @brief Retrieves the result of a callback.
   *
   * @tparam T The type of the result
   * @return T The result of the callback
   * @throws std::invalid_argument if no result is set
   */
  T getResult() {
    std::lock_guard<Module::GilAwareMutex> lock(this->callback_mutex);

    if (!this->success) {
      throw std::invalid_argument("No result set!");
    }

    return result;
  }

protected:
  T result;
};

template <> class Callback<void> : public CallbackBase {
public:
  Callback(const std::string &cb_name, const std::string &cb_signature)
      : CallbackBase(cb_name, cb_signature) {}

  /**
   * @brief Calls the callback function with the given values.
   *
   * @tparam Types The types of the values.
   * @param values The values to pass to the callback function.
   * @return bool Returns true if the callback function is called successfully,
   * false otherwise.
   */
  template <typename... Types> bool call(Types &&...values) {
    std::unique_lock<Module::GilAwareMutex> lock(this->callback_mutex);
    auto const cb = this->callback;
    lock.unlock();

    if (cb.is_none()) {
      return false;
    }

    if (DEBUG_TEST(Debug::Callback)) {
      this->begin = std::chrono::steady_clock::now();
    }

    try {
      cb(std::forward<Types>(values)...);
      this->success = true;
    } catch (py::error_already_set &e) {
      this->success = false;
      std::cerr
          << '\n'
          << "------------------------------------------------------------"
          << '\n'
          << '\n'
          << this->name << "] Error:" << std::endl;

      auto traceback = py::module_::import("traceback");
      traceback.attr("print_exception")(e.type(), e.value(), e.trace());

      std::cerr
          << "------------------------------------------------------------"
          << '\n'
          << std::endl;
      this->unset();
    }

    if (DEBUG_TEST(Debug::Callback)) {
      this->end = std::chrono::steady_clock::now();
      DEBUG_PRINT_CONDITION(
          true, Debug::Callback,
          name + "] Stats | TOTAL " +
              std::to_string(
                  std::chrono::duration_cast<std::chrono::microseconds>(
                      this->end - this->begin)
                      .count()) +
              u8" \xb5s");
    }

    return this->success;
  }

  void getResult() {}
};

} // namespace Module

#endif // C104_MODULE_CALLBACK_H
