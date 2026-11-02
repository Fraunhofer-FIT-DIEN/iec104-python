/**
 * Copyright 2026-2026 Fraunhofer Institute for Applied Information Technology
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
 * @file ProtocolParameters.h
 * @brief validated view onto the lib60870 APCI parameter block (k, w, t0..t3)
 *
 * @package iec104-python
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_PROTOCOLPARAMETERS_H
#define C104_PROTOCOLPARAMETERS_H

#include "types.h"

#include <functional>
#include <memory>
#include <string>

/**
 * @brief Validated accessor for the IEC 60870-5-104 APCI parameters of a
 * Server or Connection.
 *
 * lib60870 hands out a raw pointer into its live parameter block and uses `k`
 * as an allocation size and modulo divisor, so a zero or negative value leads
 * to a `malloc(0)`/SIGFPE in the receive thread. This class wraps that pointer
 * and
 * - enforces the IEC 60870-5-104 value ranges on every write,
 * - rejects writes while the owner is running (lib60870 reads the block from
 *   its worker threads without synchronisation),
 * - keeps the owner alive for as long as the view exists, so the raw pointer
 *   cannot dangle.
 */
class ProtocolParameters {
public:
  /// @brief minimum send window size k (IEC 60870-5-104 §9.6)
  static constexpr int K_MIN = 1;
  /// @brief maximum send window size k (IEC 60870-5-104 §9.6)
  static constexpr int K_MAX = 32767;
  /// @brief minimum receive window size w (IEC 60870-5-104 §9.6)
  static constexpr int W_MIN = 1;
  /// @brief maximum receive window size w (IEC 60870-5-104 §9.6); w must also
  /// not exceed k
  static constexpr int W_MAX = 32767;
  /// @brief minimum timeout t0..t3 in seconds
  static constexpr int T_MIN = 1;
  /// @brief maximum timeout t0, t1, t2 in seconds (IEC 60870-5-104 §9.6)
  static constexpr int T_MAX = 255;
  /// @brief maximum keep-alive interval t3 in seconds (48h, IEC 60870-5-104
  /// §9.6)
  static constexpr int T3_MAX = 172800;

  /**
   * @brief Create a validated view onto a lib60870 APCI parameter block.
   * @param parameters pointer to the live parameter block of the owner
   * @param owner handle that keeps the object owning `parameters` alive
   * @param ownerBusy predicate returning true while the owner is running, i.e.
   * while lib60870 may read the parameter block from a worker thread. May
   * reference the owner directly, since `owner` keeps it alive.
   * @param busyMessage human-readable hint appended to the error raised when a
   * write is attempted while `ownerBusy()` is true
   */
  ProtocolParameters(CS104_APCIParameters parameters,
                     std::shared_ptr<const void> owner,
                     std::function<bool()> ownerBusy, std::string busyMessage);

  ProtocolParameters(const ProtocolParameters &) = delete;
  ProtocolParameters &operator=(const ProtocolParameters &) = delete;

  /// @brief send window size k: unconfirmed outgoing I-frames before waiting
  int getSendWindowSize() const;
  /// @throws std::invalid_argument if out of range or below w
  /// @throws std::runtime_error if the owner is running
  void setSendWindowSize(int value);

  /// @brief receive window size w: unconfirmed incoming I-frames before
  /// acknowledging
  int getReceiveWindowSize() const;
  /// @throws std::invalid_argument if out of range or above k
  /// @throws std::runtime_error if the owner is running
  void setReceiveWindowSize(int value);

  /// @brief t0: socket connection timeout in seconds
  int getConnectionTimeout() const;
  /// @throws std::invalid_argument if out of range
  /// @throws std::runtime_error if the owner is running
  void setConnectionTimeout(int value);

  /// @brief t1: timeout for sent messages to be acknowledged in seconds
  int getMessageTimeout() const;
  /// @throws std::invalid_argument if out of range
  /// @throws std::runtime_error if the owner is running
  void setMessageTimeout(int value);

  /// @brief t2: maximum interval to acknowledge received messages in seconds
  int getConfirmInterval() const;
  /// @throws std::invalid_argument if out of range
  /// @throws std::runtime_error if the owner is running
  void setConfirmInterval(int value);

  /// @brief t3: maximum silent interval before sending a test frame in seconds
  int getKeepAliveInterval() const;
  /// @throws std::invalid_argument if out of range
  /// @throws std::runtime_error if the owner is running
  void setKeepAliveInterval(int value);

  /**
   * @brief the underlying lib60870 parameter block
   * @note only valid while this view is alive
   */
  CS104_APCIParameters getRaw() const { return parameters; }

  /// @brief string representation of all six values
  std::string toString() const;

  /// @brief string representation of all six values plus the block address
  std::string toRepr() const;

private:
  /// @brief live lib60870 parameter block, owned by `owner`
  CS104_APCIParameters parameters;

  /// @brief keeps the owner of `parameters` alive
  std::shared_ptr<const void> owner;

  /// @brief true while the owner's worker threads may read `parameters`
  std::function<bool()> ownerBusy;

  /// @brief hint for the error raised on writes while busy
  std::string busyMessage;

  /**
   * @brief reject writes while the owner is running
   * @param name python property name for the error message
   * @throws std::runtime_error if `ownerBusy()` is true
   */
  void assertWritable(const char *name) const;

  /**
   * @brief reject values outside [min, max]
   * @param name python property name for the error message
   * @throws std::invalid_argument if value is out of range
   */
  static void assertRange(const char *name, int value, int min, int max);
};

#endif // C104_PROTOCOLPARAMETERS_H
