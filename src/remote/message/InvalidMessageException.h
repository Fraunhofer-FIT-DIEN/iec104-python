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
 * @file InvalidMessageException.h
 * @brief 60870-5-104 incoming message parsing or validation exception
 *
 * @package iec104-python
 * @namespace Remote::Message
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_REMOTE_MESSAGE_EXCEPTION_H
#define C104_REMOTE_MESSAGE_EXCEPTION_H

#include <iostream>
#include <optional>

namespace Remote::Message {

class IncomingMessage;

class InvalidMessageException : public std::exception {
private:
  std::string what_message;
  UnexpectedMessageCause cause;
  std::shared_ptr<IncomingMessage> message;

public:
  InvalidMessageException(
      std::shared_ptr<IncomingMessage> message,
      const UnexpectedMessageCause cause,
      std::optional<const std::string> reason = std::nullopt)
      : message(message), cause(cause) {
    if (reason.has_value()) {
      what_message = "Unexpected message: " + reason.value() + " (" +
                     UnexpectedMessageCause_toString(cause) + ")";
    } else {
      what_message =
          "Unexpected message: " + UnexpectedMessageCause_toString(cause);
    }
  }

  UnexpectedMessageCause getCause() const { return cause; }

  std::shared_ptr<IncomingMessage> getMessage() const { return message; }

  std::string getWhat() const { return what_message; }

  char *what() { return what_message.data(); }
};

}; // namespace Remote::Message

#endif // C104_REMOTE_MESSAGE_EXCEPTION_H
