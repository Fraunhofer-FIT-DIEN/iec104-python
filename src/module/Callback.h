/**
 * Copyright 2020-2023 Fraunhofer Institute for Applied Information Technology
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

class CallbackBase {
public:
  CallbackBase(std::string cb_name, std::string cb_signature)
      : callback(py::none()), name(std::move(cb_name)) {
    cb_signature.erase(
        remove_if(cb_signature.begin(), cb_signature.end(), isspace),
        cb_signature.end());
    signature = std::move(cb_signature);
  }

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

  bool is_set() const {
    std::lock_guard<Module::GilAwareMutex> const lock(callback_mutex);

    return !callback.is_none();
  }

protected:
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
   * get result from callback execution
   * @return result
   * @throws std::invalid_argument if not result set
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
