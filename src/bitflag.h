/**
 * Copyright 2020-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file bitflag.h
 * @brief bit operations on enum classes
 *
 * @package iec104-python
 * @namespace Remote
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_BITFLAG_H
#define C104_BITFLAG_H

#include <ostream>
#include <type_traits> // for std::enable_if, std::is_enum, std::is_same, std::underlying_type, std::conjunction, etc.
#include <utility>     // for std::declval

/**
 * @brief Checks if any bits are set in the given enum value.
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * This function evaluates whether the provided enum value has any bits set.
 *
 * @param lhs The enum value to be checked for any bits set.
 * @return True if any bits are set in the enum value, false otherwise.
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    bool>::type
is_any(const T &lhs) {
  using underlying = std::underlying_type_t<T>;
  auto l = static_cast<underlying>(lhs);
  return l > 0;
}

/**
 * @brief Checks if no flags are set in the given enum value.
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * This function checks whether the given enum value has no bits set that
 * correspond to valid bitmask flags, as defined by the `enum_bitmask`
 * construct.
 *
 * @param lhs The enum value to check.
 * @return True if none of the defined bitmask flags are set; otherwise, false.
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    bool>::type
is_none(const T &lhs) {
  return !is_any(lhs);
}

/**
 * @brief Tests whether the bitmask representation of one enum value contains
 * another.
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * This function operates on enum types and checks if the bitwise AND operation
 * between the underlying values of `lhs` and `rhs` results in `rhs`.
 * Additionally, it ensures that neither `lhs` nor `rhs` is zero.
 *
 * @param lhs The left-hand-side enum value for the comparison.
 * @param rhs The right-hand-side enum value to check for containment in `lhs`.
 * @return true if the bitwise AND of `lhs` and `rhs` equals `rhs` and neither
 * value is zero.
 * @return false if either value is zero or the bitwise AND operation does not
 * match `rhs`.
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    bool>::type
test(const T &lhs, const T &rhs) {
  using underlying = std::underlying_type_t<T>;
  auto l = static_cast<underlying>(lhs);
  auto r = static_cast<underlying>(rhs);
  if (l == 0 || r == 0) {
    return false;
  }
  return (l & r) == r;
}

/**
 * @brief Tests whether the bitmask representation of one enum value contains
 * one of the other.
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * This function operates on enum types and checks if the bitwise AND operation
 * between the underlying values of `lhs` and `rhs` results in `rhs`.
 * Additionally, it ensures that neither `lhs` nor `rhs` is zero.
 *
 * @param lhs The left-hand-side enum value for the comparison.
 * @param rhs The right-hand-side enum value to check for containment in `lhs`.
 * @return true if the bitwise AND of `lhs` and `rhs` equals `rhs` and neither
 * value is zero.
 * @return false if either value is zero or the bitwise AND operation does not
 * match `rhs`.
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    bool>::type
any_of(const T &lhs, const T &rhs) {
  using underlying = std::underlying_type_t<T>;
  auto l = static_cast<underlying>(lhs);
  auto r = static_cast<underlying>(rhs);
  if (l == 0 || r == 0) {
    return false;
  }
  return (l & r) != 0;
}

/**
 * @brief Resets the given enum variable to its default zero-initialized state.
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs Reference to the enum variable to be reset.
 * @return A reference to the reset enum variable.
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T &>::type
reset(T &lhs) {
  lhs = static_cast<T>(0);
  return lhs;
}

/**
 * @brief bitwise AND operator
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @param rhs reference to the enum variable
 * @return operation result
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T>::type
operator&(T lhs, const T rhs) {
  using underlying = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<underlying>(lhs) &
                        static_cast<underlying>(rhs));
}

/**
 * @brief bitwise OR operator
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @param rhs reference to the enum variable
 * @return operation result
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T>::type
operator|(T lhs, const T rhs) {
  using underlying = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<underlying>(lhs) |
                        static_cast<underlying>(rhs));
}

/**
 * @brief bitwise XOR operator
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @param rhs reference to the enum variable
 * @return operation result
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T>::type
operator^(T lhs, const T rhs) {
  using underlying = std::underlying_type_t<T>;
  return static_cast<T>(static_cast<underlying>(lhs) ^
                        static_cast<underlying>(rhs));
}

/**
 * @brief bitwise NOT operator / INVERSE
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @return operation result
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T>::type
operator~(T lhs) {
  using underlying = std::underlying_type_t<T>;
  return static_cast<T>(~static_cast<underlying>(lhs));
}

/**
 * @brief bitwise AND operator, result assigned to lhs
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @param rhs reference to the enum variable
 * @return lhs
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T &>::type
operator&=(T &lhs, const T rhs) {
  lhs = lhs & rhs;
  return lhs;
}

/**
 * @brief bitwise OR operator, result assigned to lhs
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @param rhs reference to the enum variable
 * @return lhs
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T &>::type
operator|=(T &lhs, const T rhs) {
  lhs = lhs | rhs;
  return lhs;
}

/**
 * @brief bitwise XOR operator, result assigned to lhs
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param lhs reference to the enum variable
 * @param rhs reference to the enum variable
 * @return lhs
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    T &>::type
operator^=(T &lhs, const T &rhs) {
  lhs = lhs ^ rhs;
  return lhs;
}

/**
 * @brief Convert enum to string representation for output stream compatibility
 *
 * This function is enabled only for types that are enums and meet the criteria
 * defined by the `enum_bitmask` trait.
 *
 * @param os output string stream
 * @param t reference to the enum variable
 * @return lhs
 */
template <typename T>
typename std::enable_if<
    std::conjunction_v<
        std::is_enum<T>,
        std::is_same<bool, decltype(enum_bitmask(std::declval<T>()))>>,
    std::ostream &>::type
operator<<(std::ostream &os, const T &t) {
  using underlying = std::underlying_type_t<T>;
  os << std::to_string(static_cast<underlying>(t));
  return os;
}

#endif // C104_BITFLAG_H
