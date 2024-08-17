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

template <typename T, T Min, T Max, typename W> class BaseNumber {
public:
  // Constructor
  explicit BaseNumber(T v = 0) {
    if (v < Min || v > Max) {
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
    if (v < Min || v > Max) {
      throw std::out_of_range("Value is out of range.");
    }
    value = static_cast<T>(v);
  }

protected:
  T value;
  template <typename U,
            typename = std::enable_if_t<std::is_arithmetic<U>::value>>
  [[nodiscard]] T check_range(U v) const {
    if (v < Min || v > Max) {
      throw std::out_of_range("Value is out of range.");
    }
    return v;
  }
};

typedef BaseNumber<uint8_t, 0, 31, uint32_t> LimitedUInt5;
typedef BaseNumber<uint8_t, 0, 127, uint32_t> LimitedUInt7;
typedef BaseNumber<uint16_t, 0, 65535, uint32_t> LimitedUInt16;
typedef BaseNumber<int8_t, -64, 63, int32_t> LimitedInt7;
typedef BaseNumber<int16_t, -32768, 32767, int32_t> LimitedInt16;
typedef BaseNumber<float, -1.f, 1.f, double> NormalizedFloat;

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
