/**
 * Copyright 2024-2024 Fraunhofer Institute for Applied Information Technology
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
 * @file numbers.h
 * @brief allow smaller ints and floats with validation
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_NUMBERS_H
#define C104_NUMBERS_H

#include <iostream>
#include <limits>
#include <stdexcept>
#include <type_traits>

// c++17 compatible non-template approach
// First min,max option for a type as non-template version
template <typename T> struct NumberParams {
  static constexpr T min_value = static_cast<T>(0);
  static constexpr T max_value = static_cast<T>(0);
};

// Alternative min,max option for the same type as non-template version
template <typename T> struct NumberParamsAlt {
  static constexpr T min_value = static_cast<T>(0);
  static constexpr T max_value = static_cast<T>(0);
};

template <typename T, typename Params, typename W> class BaseNumber {
public:
  // Constructor
  explicit BaseNumber(T v = 0) {
    if (v < Params::min_value || v > Params::max_value) {
      throw std::out_of_range("Value is out of range.");
    }
    value = v;
  }

  explicit BaseNumber(W v) { set(v); }

  template <
      typename U = T,
      typename std::enable_if<!std::is_same<U, int32_t>::value, int>::type = 0>
  explicit BaseNumber(int32_t v) {
    set(v);
  }

  // Overloading operators with same type
  BaseNumber operator+(const BaseNumber &other) const {
    return BaseNumber(check_range(value + other.value));
  }

  BaseNumber operator-(const BaseNumber &other) const {
    return BaseNumber(check_range(value - other.value));
  }

  BaseNumber operator*(const BaseNumber &other) const {
    return BaseNumber(check_range(value * other.value));
  }

  BaseNumber operator/(const BaseNumber &other) const {
    if (other.value == 0) {
      throw std::runtime_error("Division by zero");
    }
    return BaseNumber(check_range(value / other.value));
  }

  BaseNumber &operator+=(const BaseNumber &other) {
    value = check_range(value + other.value);
    return *this;
  }

  BaseNumber &operator-=(const BaseNumber &other) {
    value = check_range(value - other.value);
    return *this;
  }

  BaseNumber &operator*=(const BaseNumber &other) {
    value = check_range(value * other.value);
    return *this;
  }

  BaseNumber &operator/=(const BaseNumber &other) {
    if (other.value == 0) {
      throw std::runtime_error("Division by zero");
    }
    value = check_range(value / other.value);
    return *this;
  }

  // Overloading operators with different types
  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber operator+(const U &other) const {
    return BaseNumber(check_range(value + other));
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber operator-(const U &other) const {
    return BaseNumber(check_range(value - other));
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber operator*(const U &other) const {
    return BaseNumber(check_range(value * other));
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber operator/(const U &other) const {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    return BaseNumber(check_range(value / other));
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber &operator+=(const U &other) {
    value = check_range(value + other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber &operator-=(const U &other) {
    value = check_range(value - other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber &operator*=(const U &other) {
    value = check_range(value * other);
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  BaseNumber &operator/=(const U &other) {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    value = check_range(value / other);
    return *this;
  }

  [[nodiscard]] T get() const { return value; }
  void set(W v) {
    if (v < Params::min_value || v > Params::max_value) {
      throw std::out_of_range("Value is out of range.");
    }
    value = static_cast<T>(v);
  }

protected:
  T value;
  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  [[nodiscard]] T check_range(U v) const {
    if (v < Params::min_value || v > Params::max_value) {
      throw std::out_of_range("Value is out of range.");
    }
    return v;
  }
};

template <> struct NumberParams<uint8_t> {
  static constexpr uint8_t min_value = 0;
  static constexpr uint8_t max_value = 31;
};
template <> struct NumberParamsAlt<uint8_t> {
  static constexpr uint8_t min_value = 0;
  static constexpr uint8_t max_value = 127;
};
template <> struct NumberParams<uint16_t> {
  static constexpr uint16_t min_value = 0;
  static constexpr uint16_t max_value = 65535;
};
template <> struct NumberParams<int8_t> {
  static constexpr int8_t min_value = -64;
  static constexpr int8_t max_value = 63;
};
template <> struct NumberParams<int16_t> {
  static constexpr int16_t min_value = -32768;
  static constexpr int16_t max_value = 32767;
};
template <> struct NumberParams<float> {
  static constexpr float min_value = -1.f;
  static constexpr float max_value = 1.f;
};

typedef BaseNumber<uint8_t, NumberParams<uint8_t>, uint32_t> LimitedUInt5;
typedef BaseNumber<uint8_t, NumberParamsAlt<uint8_t>, uint32_t> LimitedUInt7;
typedef BaseNumber<uint16_t, NumberParams<uint16_t>, uint32_t> LimitedUInt16;
typedef BaseNumber<int8_t, NumberParams<int8_t>, int32_t> LimitedInt7;
typedef BaseNumber<int16_t, NumberParams<int16_t>, int32_t> LimitedInt16;
typedef BaseNumber<float_t, NumberParams<float>, double_t> NormalizedFloat;

class Byte32 {
public:
  Byte32() : value(0) {}
  explicit Byte32(uint32_t val) : value(val) {}

  [[nodiscard]] uint32_t get() const { return value; }
  void set(uint32_t val) { value = val; }

private:
  uint32_t value;
};

#endif // C104_NUMBERS_H
