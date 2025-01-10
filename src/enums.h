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
 * @file enums.h
 * @brief shared enums, flags and string conversion of these
 *
 * @package iec104-python
 * @namespace
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_ENUMS_H
#define C104_ENUMS_H

#include <string>
#include <type_traits>

#include <cs104_connection.h>
#include <cs104_slave.h>
#include <mbedtls/ssl_ciphersuites.h>

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

/**
 * @brief IEC60870 qualifier for interrogation commands
 */
typedef enum {
  QOI_STATION = 20,
  QOI_GROUP_1 = 21,
  QOI_GROUP_2 = 22,
  QOI_GROUP_3 = 23,
  QOI_GROUP_4 = 24,
  QOI_GROUP_5 = 25,
  QOI_GROUP_6 = 26,
  QOI_GROUP_7 = 27,
  QOI_GROUP_8 = 28,
  QOI_GROUP_9 = 29,
  QOI_GROUP_10 = 30,
  QOI_GROUP_11 = 31,
  QOI_GROUP_12 = 32,
  QOI_GROUP_13 = 33,
  QOI_GROUP_14 = 34,
  QOI_GROUP_15 = 35,
  QOI_GROUP_16 = 36
} CS101_QualifierOfInterrogation;

/**
 * @brief IEC60870 qualifier for counter interrogation commands
 */
enum class CS101_QualifierOfCounterInterrogation {
  GROUP_1 = IEC60870_QCC_RQT_GROUP_1,
  GROUP_2 = IEC60870_QCC_RQT_GROUP_2,
  GROUP_3 = IEC60870_QCC_RQT_GROUP_3,
  GROUP_4 = IEC60870_QCC_RQT_GROUP_4,
  GENERAL = IEC60870_QCC_RQT_GENERAL
};

/**
 * @brief IEC60870 freeze for counter interrogation commands
 */
enum class CS101_FreezeOfCounterInterrogation {
  READ = 0,
  FREEZE_WITHOUT_RESET = 1,
  FREEZE_WITH_RESET = 2,
  COUNTER_RESET = 3
};

/**
 * @brief IEC60870 qualifier for single, double or step commands
 */
enum class CS101_QualifierOfCommand {
  NONE = IEC60870_QOC_NO_ADDITIONAL_DEFINITION,
  SHORT_PULSE = IEC60870_QOC_SHORT_PULSE_DURATION,
  LONG_PULSE = IEC60870_QOC_LONG_PULSE_DURATION,
  PERSISTENT = IEC60870_QOC_PERSISTANT_OUTPUT
};
std::string
QualifierOfCommand_toString(const CS101_QualifierOfCommand &qualifier);

/**
 * @brief IEC60870 cause of initialization reported in end-of-initialization
 * messages by RTU stations
 */
enum class CS101_CauseOfInitialization {
  LOCAL_POWER_ON = IEC60870_COI_LOCAL_SWITCH_ON,
  LOCAL_MANUAL_RESET = IEC60870_COI_LOCAL_MANUAL_RESET,
  REMOTE_RESET = IEC60870_COI_REMOTE_RESET
  // <3..31> := Reserved for future norm definitions
  // <32..127> := Reserved for user definitions (private range)
};
std::string
CauseOfInitialization_toString(const CS101_CauseOfInitialization &cause);

/**
 * @brief Enumerates the potential causes for unexpected messages
 *
 * This enumeration defines the various reasons why a received message
 * might be classified as unexpected during operation. Each enumerator
 * represents a specific cause that helps in diagnosing and handling protocol
 * issues.
 */
enum UnexpectedMessageCause {
  NO_ERROR_CAUSE,
  UNKNOWN_TYPE_ID,
  UNKNOWN_COT,
  UNKNOWN_CA,
  UNKNOWN_IOA,
  INVALID_COT,
  INVALID_TYPE_ID,
  MISMATCHED_TYPE_ID,
  UNIMPLEMENTED_GROUP
};
std::string UnexpectedMessageCause_toString(const UnexpectedMessageCause &mode);

/**
 * @brief Represents various debugging levels as enum_bitmask
 *
 * This enum class defines debugging categories that can be used to specify
 * and manage different areas of debugging in an application. Each category
 * is represented by a single bit and can be combined using bitwise operations.
 */
enum class Debug : uint8_t {
  None = 0,
  Server = 0x01,
  Client = 0x02,
  Connection = 0x04,
  Station = 0x08,
  Point = 0x10,
  Message = 0x20,
  Callback = 0x40,
  Gil = 0x80,
  All = 0xFF
};
constexpr bool enum_bitmask(Debug &&);
std::string Debug_toString(const Debug &mode);
std::string Debug_toFlagString(const Debug &mode);

/**
 * @brief IEC60870 quality attributes as enum_bitmask
 *
 * Transmitted measurement values can be flagged to indicate their quality.
 */
enum class Quality {
  None = 0,
  Overflow = IEC60870_QUALITY_OVERFLOW, // only in sp, dp
  ElapsedTimeInvalid =
      IEC60870_QUALITY_ELAPSED_TIME_INVALID, // only equipment protection
  Blocked = IEC60870_QUALITY_BLOCKED,
  Substituted = IEC60870_QUALITY_SUBSTITUTED,
  NonTopical = IEC60870_QUALITY_NON_TOPICAL,
  Invalid = IEC60870_QUALITY_INVALID
};
constexpr bool enum_bitmask(Quality &&);
std::string Quality_toString(const Quality &quality);

/**
 * @brief IEC60870 quality attributes of a binary counter value as enum_bitmask
 *
 * Transmitted counter values can be flagged to indicate their quality.
 */
enum class BinaryCounterQuality {
  None = 0,
  Adjusted = 0x20,
  Carry = 0x40,
  Invalid = 0x80
};
constexpr bool enum_bitmask(BinaryCounterQuality &&);
std::string BinaryCounterQuality_toString(const BinaryCounterQuality &quality);

/**
 * @brief IEC60870 equipment protection start events as enum_bitmask
 */
enum class StartEvents {
  None = 0,
  General = IEC60870_START_EVENT_GS,
  PhaseL1 = IEC60870_START_EVENT_SL1,
  PhaseL2 = IEC60870_START_EVENT_SL2,
  PhaseL3 = IEC60870_START_EVENT_SL3,
  InEarthCurrent = IEC60870_START_EVENT_SIE,
  ReverseDirection = IEC60870_START_EVENT_SRD,
};
constexpr bool enum_bitmask(StartEvents &&);
std::string StartEvents_toString(const StartEvents &events);

/**
 * @brief IEC60870 equipment protection output circuits as enum_bitmask
 */
enum class OutputCircuits {
  None = 0,
  General = IEC60870_OUTPUT_CI_GC,
  PhaseL1 = IEC60870_OUTPUT_CI_CL1,
  PhaseL2 = IEC60870_OUTPUT_CI_CL2,
  PhaseL3 = IEC60870_OUTPUT_CI_CL3
};
constexpr bool enum_bitmask(OutputCircuits &&);
std::string OutputCircuits_toString(const OutputCircuits &infos);

/**
 * @brief generic 16 bit enum_bitmask used in IEC60870 status with change
 * detection
 */
enum class FieldSet16 {
  None = 0x0000,
  I0 = 0x0001,
  I1 = 0x0002,
  I2 = 0x0004,
  I3 = 0x0008,
  I4 = 0x0010,
  I5 = 0x0020,
  I6 = 0x0040,
  I7 = 0x0080,
  I8 = 0x0100,
  I9 = 0x0200,
  I10 = 0x0400,
  I11 = 0x0800,
  I12 = 0x1000,
  I13 = 0x2000,
  I14 = 0x4000,
  I15 = 0x8000
};
constexpr bool enum_bitmask(FieldSet16 &&);
std::string FieldSet16_toString(const FieldSet16 &infos);

/**
 * @brief link states for connection state machine behaviour
 */
enum ConnectionState {
  CLOSED,
  CLOSED_AWAIT_OPEN,
  CLOSED_AWAIT_RECONNECT,
  OPEN_MUTED,
  OPEN,
  OPEN_AWAIT_CLOSED
};
std::string ConnectionState_toString(const ConnectionState &state);

std::string ConnectionEvent_toString(const CS104_ConnectionEvent &event);

std::string
PeerConnectionEvent_toString(const CS104_PeerConnectionEvent &event);

std::string DoublePointValue_toString(const DoublePointValue &value);

std::string StepCommandValue_toString(const StepCommandValue &value);

std::string EventState_toString(const EventState &state);

/**
 * @brief initial communication sequences send to a connection that starts data
 * transmission
 */
enum ConnectionInit {
  INIT_ALL,
  INIT_INTERROGATION,
  INIT_CLOCK_SYNC,
  INIT_MUTED,
  INIT_NONE
};
std::string ConnectionInit_toString(const ConnectionInit &init);

/**
 * @brief command response states, control servers response behaviour with
 * python callbacks return value
 */
enum CommandResponseState {
  RESPONSE_STATE_FAILURE,
  RESPONSE_STATE_SUCCESS,
  RESPONSE_STATE_NONE
};
std::string CommandResponseState_toString(const CommandResponseState &state);

/**
 * @brief command processing progress states
 */
enum CommandProcessState {
  COMMAND_FAILURE,
  COMMAND_SUCCESS,
  COMMAND_AWAIT_CON,
  COMMAND_AWAIT_TERM,
  COMMAND_AWAIT_CON_TERM,
  COMMAND_AWAIT_REQUEST
};
std::string CommandProcessState_toString(const CommandProcessState &state);

/**
 * @brief command transmission modes (execute directly or select before execute
 * for unique control access)
 */
enum CommandTransmissionMode {
  DIRECT_COMMAND,
  SELECT_AND_EXECUTE_COMMAND,
};
std::string
CommandTransmissionMode_toString(const CommandTransmissionMode &mode);

/**
 * @brief Represents supported TLS cipher suites mapped to their corresponding
 * identifiers.
 *
 * This enumeration provides a comprehensive list of TLS cipher suites supported
 * by the system. Each cipher suite maps directly to a corresponding
 * MBEDTLS-defined constant, which enables secure communication in compliance
 * with the TLS protocol.
 */
enum class TLSCipherSuite {
  RSA_WITH_NULL_MD5 = MBEDTLS_TLS_RSA_WITH_NULL_MD5,
  RSA_WITH_NULL_SHA = MBEDTLS_TLS_RSA_WITH_NULL_SHA,
  PSK_WITH_NULL_SHA = MBEDTLS_TLS_PSK_WITH_NULL_SHA,
  DHE_PSK_WITH_NULL_SHA = MBEDTLS_TLS_DHE_PSK_WITH_NULL_SHA,
  RSA_PSK_WITH_NULL_SHA = MBEDTLS_TLS_RSA_PSK_WITH_NULL_SHA,
  RSA_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA,
  DHE_RSA_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_DHE_RSA_WITH_AES_128_CBC_SHA,
  RSA_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA,
  DHE_RSA_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_DHE_RSA_WITH_AES_256_CBC_SHA,
  RSA_WITH_NULL_SHA256 = MBEDTLS_TLS_RSA_WITH_NULL_SHA256,
  RSA_WITH_AES_128_CBC_SHA256 = MBEDTLS_TLS_RSA_WITH_AES_128_CBC_SHA256,
  RSA_WITH_AES_256_CBC_SHA256 = MBEDTLS_TLS_RSA_WITH_AES_256_CBC_SHA256,
  RSA_WITH_CAMELLIA_128_CBC_SHA = MBEDTLS_TLS_RSA_WITH_CAMELLIA_128_CBC_SHA,
  DHE_RSA_WITH_CAMELLIA_128_CBC_SHA =
      MBEDTLS_TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA,
  DHE_RSA_WITH_AES_128_CBC_SHA256 = MBEDTLS_TLS_DHE_RSA_WITH_AES_128_CBC_SHA256,
  DHE_RSA_WITH_AES_256_CBC_SHA256 = MBEDTLS_TLS_DHE_RSA_WITH_AES_256_CBC_SHA256,
  RSA_WITH_CAMELLIA_256_CBC_SHA = MBEDTLS_TLS_RSA_WITH_CAMELLIA_256_CBC_SHA,
  DHE_RSA_WITH_CAMELLIA_256_CBC_SHA =
      MBEDTLS_TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA,
  PSK_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA,
  PSK_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA,
  DHE_PSK_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_DHE_PSK_WITH_AES_128_CBC_SHA,
  DHE_PSK_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_DHE_PSK_WITH_AES_256_CBC_SHA,
  RSA_PSK_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_RSA_PSK_WITH_AES_128_CBC_SHA,
  RSA_PSK_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_RSA_PSK_WITH_AES_256_CBC_SHA,
  RSA_WITH_AES_128_GCM_SHA256 = MBEDTLS_TLS_RSA_WITH_AES_128_GCM_SHA256,
  RSA_WITH_AES_256_GCM_SHA384 = MBEDTLS_TLS_RSA_WITH_AES_256_GCM_SHA384,
  DHE_RSA_WITH_AES_128_GCM_SHA256 = MBEDTLS_TLS_DHE_RSA_WITH_AES_128_GCM_SHA256,
  DHE_RSA_WITH_AES_256_GCM_SHA384 = MBEDTLS_TLS_DHE_RSA_WITH_AES_256_GCM_SHA384,
  PSK_WITH_AES_128_GCM_SHA256 = MBEDTLS_TLS_PSK_WITH_AES_128_GCM_SHA256,
  PSK_WITH_AES_256_GCM_SHA384 = MBEDTLS_TLS_PSK_WITH_AES_256_GCM_SHA384,
  DHE_PSK_WITH_AES_128_GCM_SHA256 = MBEDTLS_TLS_DHE_PSK_WITH_AES_128_GCM_SHA256,
  DHE_PSK_WITH_AES_256_GCM_SHA384 = MBEDTLS_TLS_DHE_PSK_WITH_AES_256_GCM_SHA384,
  RSA_PSK_WITH_AES_128_GCM_SHA256 = MBEDTLS_TLS_RSA_PSK_WITH_AES_128_GCM_SHA256,
  RSA_PSK_WITH_AES_256_GCM_SHA384 = MBEDTLS_TLS_RSA_PSK_WITH_AES_256_GCM_SHA384,
  PSK_WITH_AES_128_CBC_SHA256 = MBEDTLS_TLS_PSK_WITH_AES_128_CBC_SHA256,
  PSK_WITH_AES_256_CBC_SHA384 = MBEDTLS_TLS_PSK_WITH_AES_256_CBC_SHA384,
  PSK_WITH_NULL_SHA256 = MBEDTLS_TLS_PSK_WITH_NULL_SHA256,
  PSK_WITH_NULL_SHA384 = MBEDTLS_TLS_PSK_WITH_NULL_SHA384,
  DHE_PSK_WITH_AES_128_CBC_SHA256 = MBEDTLS_TLS_DHE_PSK_WITH_AES_128_CBC_SHA256,
  DHE_PSK_WITH_AES_256_CBC_SHA384 = MBEDTLS_TLS_DHE_PSK_WITH_AES_256_CBC_SHA384,
  DHE_PSK_WITH_NULL_SHA256 = MBEDTLS_TLS_DHE_PSK_WITH_NULL_SHA256,
  DHE_PSK_WITH_NULL_SHA384 = MBEDTLS_TLS_DHE_PSK_WITH_NULL_SHA384,
  RSA_PSK_WITH_AES_128_CBC_SHA256 = MBEDTLS_TLS_RSA_PSK_WITH_AES_128_CBC_SHA256,
  RSA_PSK_WITH_AES_256_CBC_SHA384 = MBEDTLS_TLS_RSA_PSK_WITH_AES_256_CBC_SHA384,
  RSA_PSK_WITH_NULL_SHA256 = MBEDTLS_TLS_RSA_PSK_WITH_NULL_SHA256,
  RSA_PSK_WITH_NULL_SHA384 = MBEDTLS_TLS_RSA_PSK_WITH_NULL_SHA384,
  RSA_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_RSA_WITH_CAMELLIA_128_CBC_SHA256,
  DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_DHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
  RSA_WITH_CAMELLIA_256_CBC_SHA256 =
      MBEDTLS_TLS_RSA_WITH_CAMELLIA_256_CBC_SHA256,
  DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256 =
      MBEDTLS_TLS_DHE_RSA_WITH_CAMELLIA_256_CBC_SHA256,
  ECDH_ECDSA_WITH_NULL_SHA = MBEDTLS_TLS_ECDH_ECDSA_WITH_NULL_SHA,
  ECDH_ECDSA_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA,
  ECDH_ECDSA_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA,
  ECDHE_ECDSA_WITH_NULL_SHA = MBEDTLS_TLS_ECDHE_ECDSA_WITH_NULL_SHA,
  ECDHE_ECDSA_WITH_AES_128_CBC_SHA =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA,
  ECDHE_ECDSA_WITH_AES_256_CBC_SHA =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA,
  ECDH_RSA_WITH_NULL_SHA = MBEDTLS_TLS_ECDH_RSA_WITH_NULL_SHA,
  ECDH_RSA_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA,
  ECDH_RSA_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA,
  ECDHE_RSA_WITH_NULL_SHA = MBEDTLS_TLS_ECDHE_RSA_WITH_NULL_SHA,
  ECDHE_RSA_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA,
  ECDHE_RSA_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA,
  ECDHE_ECDSA_WITH_AES_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CBC_SHA256,
  ECDHE_ECDSA_WITH_AES_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CBC_SHA384,
  ECDH_ECDSA_WITH_AES_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_CBC_SHA256,
  ECDH_ECDSA_WITH_AES_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_256_CBC_SHA384,
  ECDHE_RSA_WITH_AES_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_CBC_SHA256,
  ECDHE_RSA_WITH_AES_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_CBC_SHA384,
  ECDH_RSA_WITH_AES_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_CBC_SHA256,
  ECDH_RSA_WITH_AES_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDH_RSA_WITH_AES_256_CBC_SHA384,
  ECDHE_ECDSA_WITH_AES_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_GCM_SHA256,
  ECDHE_ECDSA_WITH_AES_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_GCM_SHA384,
  ECDH_ECDSA_WITH_AES_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_128_GCM_SHA256,
  ECDH_ECDSA_WITH_AES_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_AES_256_GCM_SHA384,
  ECDHE_RSA_WITH_AES_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_128_GCM_SHA256,
  ECDHE_RSA_WITH_AES_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_AES_256_GCM_SHA384,
  ECDH_RSA_WITH_AES_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDH_RSA_WITH_AES_128_GCM_SHA256,
  ECDH_RSA_WITH_AES_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDH_RSA_WITH_AES_256_GCM_SHA384,
  ECDHE_PSK_WITH_AES_128_CBC_SHA = MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA,
  ECDHE_PSK_WITH_AES_256_CBC_SHA = MBEDTLS_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA,
  ECDHE_PSK_WITH_AES_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_AES_128_CBC_SHA256,
  ECDHE_PSK_WITH_AES_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_AES_256_CBC_SHA384,
  ECDHE_PSK_WITH_NULL_SHA = MBEDTLS_TLS_ECDHE_PSK_WITH_NULL_SHA,
  ECDHE_PSK_WITH_NULL_SHA256 = MBEDTLS_TLS_ECDHE_PSK_WITH_NULL_SHA256,
  ECDHE_PSK_WITH_NULL_SHA384 = MBEDTLS_TLS_ECDHE_PSK_WITH_NULL_SHA384,
  RSA_WITH_ARIA_128_CBC_SHA256 = MBEDTLS_TLS_RSA_WITH_ARIA_128_CBC_SHA256,
  RSA_WITH_ARIA_256_CBC_SHA384 = MBEDTLS_TLS_RSA_WITH_ARIA_256_CBC_SHA384,
  DHE_RSA_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_DHE_RSA_WITH_ARIA_128_CBC_SHA256,
  DHE_RSA_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_DHE_RSA_WITH_ARIA_256_CBC_SHA384,
  ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_128_CBC_SHA256,
  ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_256_CBC_SHA384,
  ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_ARIA_128_CBC_SHA256,
  ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_ARIA_256_CBC_SHA384,
  ECDHE_RSA_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_128_CBC_SHA256,
  ECDHE_RSA_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_256_CBC_SHA384,
  ECDH_RSA_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDH_RSA_WITH_ARIA_128_CBC_SHA256,
  ECDH_RSA_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDH_RSA_WITH_ARIA_256_CBC_SHA384,
  RSA_WITH_ARIA_128_GCM_SHA256 = MBEDTLS_TLS_RSA_WITH_ARIA_128_GCM_SHA256,
  RSA_WITH_ARIA_256_GCM_SHA384 = MBEDTLS_TLS_RSA_WITH_ARIA_256_GCM_SHA384,
  DHE_RSA_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_DHE_RSA_WITH_ARIA_128_GCM_SHA256,
  DHE_RSA_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_DHE_RSA_WITH_ARIA_256_GCM_SHA384,
  ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_128_GCM_SHA256,
  ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_ARIA_256_GCM_SHA384,
  ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_ARIA_128_GCM_SHA256,
  ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_ARIA_256_GCM_SHA384,
  ECDHE_RSA_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_128_GCM_SHA256,
  ECDHE_RSA_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_ARIA_256_GCM_SHA384,
  ECDH_RSA_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDH_RSA_WITH_ARIA_128_GCM_SHA256,
  ECDH_RSA_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDH_RSA_WITH_ARIA_256_GCM_SHA384,
  PSK_WITH_ARIA_128_CBC_SHA256 = MBEDTLS_TLS_PSK_WITH_ARIA_128_CBC_SHA256,
  PSK_WITH_ARIA_256_CBC_SHA384 = MBEDTLS_TLS_PSK_WITH_ARIA_256_CBC_SHA384,
  DHE_PSK_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_DHE_PSK_WITH_ARIA_128_CBC_SHA256,
  DHE_PSK_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_DHE_PSK_WITH_ARIA_256_CBC_SHA384,
  RSA_PSK_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_RSA_PSK_WITH_ARIA_128_CBC_SHA256,
  RSA_PSK_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_RSA_PSK_WITH_ARIA_256_CBC_SHA384,
  PSK_WITH_ARIA_128_GCM_SHA256 = MBEDTLS_TLS_PSK_WITH_ARIA_128_GCM_SHA256,
  PSK_WITH_ARIA_256_GCM_SHA384 = MBEDTLS_TLS_PSK_WITH_ARIA_256_GCM_SHA384,
  DHE_PSK_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_DHE_PSK_WITH_ARIA_128_GCM_SHA256,
  DHE_PSK_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_DHE_PSK_WITH_ARIA_256_GCM_SHA384,
  RSA_PSK_WITH_ARIA_128_GCM_SHA256 =
      MBEDTLS_TLS_RSA_PSK_WITH_ARIA_128_GCM_SHA256,
  RSA_PSK_WITH_ARIA_256_GCM_SHA384 =
      MBEDTLS_TLS_RSA_PSK_WITH_ARIA_256_GCM_SHA384,
  ECDHE_PSK_WITH_ARIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_ARIA_128_CBC_SHA256,
  ECDHE_PSK_WITH_ARIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_ARIA_256_CBC_SHA384,
  ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
  ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
  ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_CAMELLIA_128_CBC_SHA256,
  ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_CAMELLIA_256_CBC_SHA384,
  ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_128_CBC_SHA256,
  ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_256_CBC_SHA384,
  ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDH_RSA_WITH_CAMELLIA_128_CBC_SHA256,
  ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDH_RSA_WITH_CAMELLIA_256_CBC_SHA384,
  RSA_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_RSA_WITH_CAMELLIA_128_GCM_SHA256,
  RSA_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_RSA_WITH_CAMELLIA_256_GCM_SHA384,
  DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_DHE_RSA_WITH_CAMELLIA_128_GCM_SHA256,
  DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_DHE_RSA_WITH_CAMELLIA_256_GCM_SHA384,
  ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_128_GCM_SHA256,
  ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CAMELLIA_256_GCM_SHA384,
  ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_CAMELLIA_128_GCM_SHA256,
  ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDH_ECDSA_WITH_CAMELLIA_256_GCM_SHA384,
  ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_128_GCM_SHA256,
  ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_CAMELLIA_256_GCM_SHA384,
  ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_ECDH_RSA_WITH_CAMELLIA_128_GCM_SHA256,
  ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_ECDH_RSA_WITH_CAMELLIA_256_GCM_SHA384,
  PSK_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_PSK_WITH_CAMELLIA_128_GCM_SHA256,
  PSK_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_PSK_WITH_CAMELLIA_256_GCM_SHA384,
  DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_DHE_PSK_WITH_CAMELLIA_128_GCM_SHA256,
  DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_DHE_PSK_WITH_CAMELLIA_256_GCM_SHA384,
  RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256 =
      MBEDTLS_TLS_RSA_PSK_WITH_CAMELLIA_128_GCM_SHA256,
  RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384 =
      MBEDTLS_TLS_RSA_PSK_WITH_CAMELLIA_256_GCM_SHA384,
  PSK_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_PSK_WITH_CAMELLIA_128_CBC_SHA256,
  PSK_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_PSK_WITH_CAMELLIA_256_CBC_SHA384,
  DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_DHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
  DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_DHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
  RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_RSA_PSK_WITH_CAMELLIA_128_CBC_SHA256,
  RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_RSA_PSK_WITH_CAMELLIA_256_CBC_SHA384,
  ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_CAMELLIA_128_CBC_SHA256,
  ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_CAMELLIA_256_CBC_SHA384,
  RSA_WITH_AES_128_CCM = MBEDTLS_TLS_RSA_WITH_AES_128_CCM,
  RSA_WITH_AES_256_CCM = MBEDTLS_TLS_RSA_WITH_AES_256_CCM,
  DHE_RSA_WITH_AES_128_CCM = MBEDTLS_TLS_DHE_RSA_WITH_AES_128_CCM,
  DHE_RSA_WITH_AES_256_CCM = MBEDTLS_TLS_DHE_RSA_WITH_AES_256_CCM,
  RSA_WITH_AES_128_CCM_8 = MBEDTLS_TLS_RSA_WITH_AES_128_CCM_8,
  RSA_WITH_AES_256_CCM_8 = MBEDTLS_TLS_RSA_WITH_AES_256_CCM_8,
  DHE_RSA_WITH_AES_128_CCM_8 = MBEDTLS_TLS_DHE_RSA_WITH_AES_128_CCM_8,
  DHE_RSA_WITH_AES_256_CCM_8 = MBEDTLS_TLS_DHE_RSA_WITH_AES_256_CCM_8,
  PSK_WITH_AES_128_CCM = MBEDTLS_TLS_PSK_WITH_AES_128_CCM,
  PSK_WITH_AES_256_CCM = MBEDTLS_TLS_PSK_WITH_AES_256_CCM,
  DHE_PSK_WITH_AES_128_CCM = MBEDTLS_TLS_DHE_PSK_WITH_AES_128_CCM,
  DHE_PSK_WITH_AES_256_CCM = MBEDTLS_TLS_DHE_PSK_WITH_AES_256_CCM,
  PSK_WITH_AES_128_CCM_8 = MBEDTLS_TLS_PSK_WITH_AES_128_CCM_8,
  PSK_WITH_AES_256_CCM_8 = MBEDTLS_TLS_PSK_WITH_AES_256_CCM_8,
  DHE_PSK_WITH_AES_128_CCM_8 = MBEDTLS_TLS_DHE_PSK_WITH_AES_128_CCM_8,
  DHE_PSK_WITH_AES_256_CCM_8 = MBEDTLS_TLS_DHE_PSK_WITH_AES_256_CCM_8,
  ECDHE_ECDSA_WITH_AES_128_CCM = MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM,
  ECDHE_ECDSA_WITH_AES_256_CCM = MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CCM,
  ECDHE_ECDSA_WITH_AES_128_CCM_8 = MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_128_CCM_8,
  ECDHE_ECDSA_WITH_AES_256_CCM_8 = MBEDTLS_TLS_ECDHE_ECDSA_WITH_AES_256_CCM_8,
  ECJPAKE_WITH_AES_128_CCM_8 = MBEDTLS_TLS_ECJPAKE_WITH_AES_128_CCM_8,
  ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
  ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256,
  DHE_RSA_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_DHE_RSA_WITH_CHACHA20_POLY1305_SHA256,
  PSK_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_PSK_WITH_CHACHA20_POLY1305_SHA256,
  ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_ECDHE_PSK_WITH_CHACHA20_POLY1305_SHA256,
  DHE_PSK_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_DHE_PSK_WITH_CHACHA20_POLY1305_SHA256,
  RSA_PSK_WITH_CHACHA20_POLY1305_SHA256 =
      MBEDTLS_TLS_RSA_PSK_WITH_CHACHA20_POLY1305_SHA256,
  TLS1_3_AES_128_GCM_SHA256 = MBEDTLS_TLS1_3_AES_128_GCM_SHA256,
  TLS1_3_AES_256_GCM_SHA384 = MBEDTLS_TLS1_3_AES_256_GCM_SHA384,
  TLS1_3_CHACHA20_POLY1305_SHA256 = MBEDTLS_TLS1_3_CHACHA20_POLY1305_SHA256,
  TLS1_3_AES_128_CCM_SHA256 = MBEDTLS_TLS1_3_AES_128_CCM_SHA256,
  TLS1_3_AES_128_CCM_8_SHA256 = MBEDTLS_TLS1_3_AES_128_CCM_8_SHA256
};
std::string TLSCipherSuite_toString(const TLSCipherSuite &cipher);

#endif // C104_ENUMS_H
