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
 * @file ProtocolParameters.cpp
 * @brief validated view onto the lib60870 APCI parameter block (k, w, t0..t3)
 *
 * @package iec104-python
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#include "ProtocolParameters.h"

#include <cstdint>
#include <sstream>
#include <stdexcept>
#include <utility>

ProtocolParameters::ProtocolParameters(CS104_APCIParameters parameters,
                                       std::shared_ptr<const void> owner,
                                       std::function<bool()> ownerBusy,
                                       std::string busyMessage)
    : parameters(parameters), owner(std::move(owner)),
      ownerBusy(std::move(ownerBusy)), busyMessage(std::move(busyMessage)) {
  if (!this->parameters) {
    throw std::invalid_argument("ProtocolParameters: parameter block is null");
  }
  if (!this->ownerBusy) {
    throw std::invalid_argument("ProtocolParameters: busy predicate is empty");
  }
}

void ProtocolParameters::assertWritable(const char *name) const {
  if (ownerBusy()) {
    throw std::runtime_error(std::string("Cannot modify protocol parameter ") +
                             name + " " + busyMessage);
  }
}

void ProtocolParameters::assertRange(const char *name, const int value,
                                     const int min, const int max) {
  if (value < min || value > max) {
    throw std::invalid_argument(std::string("Protocol parameter ") + name +
                                " must be in range [" + std::to_string(min) +
                                ", " + std::to_string(max) + "], got " +
                                std::to_string(value));
  }
}

int ProtocolParameters::getSendWindowSize() const { return parameters->k; }

void ProtocolParameters::setSendWindowSize(const int value) {
  assertWritable("send_window_size");
  assertRange("send_window_size", value, K_MIN, K_MAX);
  if (value < parameters->w) {
    throw std::invalid_argument(
        "Protocol parameter send_window_size (k) must not be smaller than "
        "receive_window_size (w=" +
        std::to_string(parameters->w) + "), got " + std::to_string(value));
  }
  parameters->k = value;
}

int ProtocolParameters::getReceiveWindowSize() const { return parameters->w; }

void ProtocolParameters::setReceiveWindowSize(const int value) {
  assertWritable("receive_window_size");
  assertRange("receive_window_size", value, W_MIN, W_MAX);
  if (value > parameters->k) {
    throw std::invalid_argument(
        "Protocol parameter receive_window_size (w) must not exceed "
        "send_window_size (k=" +
        std::to_string(parameters->k) + "), got " + std::to_string(value));
  }
  parameters->w = value;
}

int ProtocolParameters::getConnectionTimeout() const { return parameters->t0; }

void ProtocolParameters::setConnectionTimeout(const int value) {
  assertWritable("connection_timeout");
  assertRange("connection_timeout", value, T_MIN, T_MAX);
  parameters->t0 = value;
}

int ProtocolParameters::getMessageTimeout() const { return parameters->t1; }

void ProtocolParameters::setMessageTimeout(const int value) {
  assertWritable("message_timeout");
  assertRange("message_timeout", value, T_MIN, T_MAX);
  parameters->t1 = value;
}

int ProtocolParameters::getConfirmInterval() const { return parameters->t2; }

void ProtocolParameters::setConfirmInterval(const int value) {
  assertWritable("confirm_interval");
  assertRange("confirm_interval", value, T_MIN, T_MAX);
  parameters->t2 = value;
}

int ProtocolParameters::getKeepAliveInterval() const { return parameters->t3; }

void ProtocolParameters::setKeepAliveInterval(const int value) {
  assertWritable("keep_alive_interval");
  assertRange("keep_alive_interval", value, T_MIN, T3_MAX);
  parameters->t3 = value;
}

std::string ProtocolParameters::toString() const {
  std::ostringstream oss;
  oss << "<104.ProtocolParameters send_window_size=" << parameters->k
      << ", receive_window_size=" << parameters->w
      << ", connection_timeout=" << parameters->t0
      << ", message_timeout=" << parameters->t1
      << ", confirm_interval=" << parameters->t2
      << ", keep_alive_interval=" << parameters->t3 << ">";
  return oss.str();
}

std::string ProtocolParameters::toRepr() const {
  std::ostringstream oss;
  oss << "<104.ProtocolParameters send_window_size=" << parameters->k
      << ", receive_window_size=" << parameters->w
      << ", connection_timeout=" << parameters->t0
      << ", message_timeout=" << parameters->t1
      << ", confirm_interval=" << parameters->t2
      << ", keep_alive_interval=" << parameters->t3 << " at " << std::hex
      << std::showbase << reinterpret_cast<std::uintptr_t>(parameters) << ">";
  return oss.str();
}
