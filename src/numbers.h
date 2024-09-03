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

/**
 * @brief integer representation with special limits
 */
template <typename T> class LimitedInteger {
public:
  // Constructor
  LimitedInteger() = default;

  explicit LimitedInteger(int v) { throw std::domain_error("using base ctor"); }

  [[nodiscard]] virtual int getMin() const { return 0; }

  [[nodiscard]] virtual int getMax() const { return 0; }

  // Overloading operators with different types
  virtual int operator+(const int &other) const { return value + other; }

  virtual int operator-(const int &other) const { return value - other; }

  virtual int operator*(const int &other) const { return value * other; }

  virtual int operator/(const int &other) const {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    return value / other;
  }

  LimitedInteger &operator+=(const int &other) {
    value = check_range(value + other);
    return *this;
  }

  LimitedInteger &operator-=(const int &other) {
    value = check_range(value - other);
    return *this;
  }

  LimitedInteger &operator*=(const int &other) {
    value = check_range(value * other);
    return *this;
  }

  LimitedInteger &operator/=(const int &other) {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    value = check_range(value / other);
    return *this;
  }

  [[nodiscard]] T get() const { return value; }

  void set(int v) { value = check_range(v); }

protected:
  T value{0};

  [[nodiscard]] T check_range(int v) const {
    if (v < getMin() || v > getMax()) {
      throw std::out_of_range("Value is out of range.");
    }
    return v;
  }
};

/**
 * @brief unsigned integer of 5 bits size (0 - 31)
 */
class LimitedUInt5 : public LimitedInteger<uint8_t> {
public:
  // Constructor
  LimitedUInt5() = default;

  explicit LimitedUInt5(int v) { set(v); }

  [[nodiscard]] int getMin() const override { return 0; }

  [[nodiscard]] int getMax() const override { return 31; }
};

/**
 * @brief unsigned integer of 7 bits size (0 - 127)
 */
class LimitedUInt7 : public LimitedInteger<uint8_t> {
public:
  // Constructor
  LimitedUInt7() = default;

  explicit LimitedUInt7(int v) { set(v); }

  [[nodiscard]] int getMin() const override { return 0; }

  [[nodiscard]] int getMax() const override { return 127; }
};

/**
 * @brief unsigned integer of 16 bits size (0 - 65535)
 */
class LimitedUInt16 : public LimitedInteger<uint16_t> {
public:
  // Constructor
  LimitedUInt16() = default;

  explicit LimitedUInt16(int v) { set(v); }

  [[nodiscard]] int getMin() const override { return 0; }

  [[nodiscard]] int getMax() const override { return 65535; }
};

/**
 * @brief signed integer of 7 bits size (-64 - 63)
 */
class LimitedInt7 : public LimitedInteger<int8_t> {
public:
  // Constructor
  LimitedInt7() = default;

  explicit LimitedInt7(int v) { set(v); }

  [[nodiscard]] int getMin() const override { return -64; }

  [[nodiscard]] int getMax() const override { return 63; }
};

/**
 * @brief signed integer of 16 bits size (-32768 - 32767)
 */
class LimitedInt16 : public LimitedInteger<int16_t> {
public:
  // Constructor
  LimitedInt16() = default;

  explicit LimitedInt16(int v) { set(v); }

  [[nodiscard]] int getMin() const override { return -32768; }

  [[nodiscard]] int getMax() const override { return 32767; }
};

/**
 * @brief normalized floating point value of 32 bits size (-1.0 - 1.0)
 */
class NormalizedFloat {
public:
  // Constructor
  NormalizedFloat() = default;

  explicit NormalizedFloat(int v) { set(v); }

  explicit NormalizedFloat(float v) { set(v); }

  [[nodiscard]] float getMin() const { return -1.f; }

  [[nodiscard]] float getMax() const { return 1.f; }

  // Overloading operators with different types
  float operator+(const int &other) const { return value + other; }

  float operator-(const int &other) const { return value - other; }

  float operator*(const int &other) const { return value * other; }

  float operator/(const int &other) const {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    return value / other;
  }

  float operator+(const float &other) const { return value + other; }

  float operator-(const float &other) const { return value - other; }

  float operator*(const float &other) const { return value * other; }

  float operator/(const float &other) const {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    return value / other;
  }

  NormalizedFloat &operator+=(const int &other) {
    value = check_range(value + other);
    return *this;
  }

  NormalizedFloat &operator-=(const int &other) {
    value = check_range(value - other);
    return *this;
  }

  NormalizedFloat &operator*=(const int &other) {
    value = check_range(value * other);
    return *this;
  }

  NormalizedFloat &operator/=(const int &other) {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    value = check_range(value / other);
    return *this;
  }

  NormalizedFloat &operator+=(const float &other) {
    value = check_range(value + other);
    return *this;
  }

  NormalizedFloat &operator-=(const float &other) {
    value = check_range(value - other);
    return *this;
  }

  NormalizedFloat &operator*=(const float &other) {
    value = check_range(value * other);
    return *this;
  }

  NormalizedFloat &operator/=(const float &other) {
    if (other == 0) {
      throw std::runtime_error("Division by zero");
    }
    value = check_range(value / other);
    return *this;
  }

  [[nodiscard]] float get() const { return value; }

  void set(float v) { value = check_range(v); }

protected:
  float value{0};

  [[nodiscard]] float check_range(float v) const {
    if (v < getMin() || v > getMax()) {
      throw std::out_of_range("Value is out of range.");
    }
    return v;
  }
};

/**
 * @brief raw bytes of 32 bits size
 */
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
