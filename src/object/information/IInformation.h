/**
 * Copyright 2024-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file AbstractInformation.h
 * @brief information object base
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_INTERFACE_H
#define C104_OBJECT_INFORMATION_INTERFACE_H

#include <mutex>
#include <optional>
#include <utility>

#include <cs101_information_objects.h>

#include "enums.h"
#include "numbers.h"
#include "object/DateTime.h"
#include "types.h"
#include "variadic.h"

namespace Object::Information {

class IInformation : public std::enable_shared_from_this<IInformation> {
protected:
  mutable std::recursive_mutex mtx;

  /// @brief timestamp of information value generation, optional
  std::optional<Object::DateTime> recorded_at;

  /// @brief timestamp of last local processing (sending or receiving)
  Object::DateTime processed_at;

  /// @brief toggle, if modification is allowed or not
  bool readonly;

  /// @brief retrieve the primary value property - must be implemented by child
  /// classes
  [[nodiscard]] virtual InfoValue getValueImpl() const = 0;

  /// @brief update the primary value property - must be implemented by child
  /// classes
  virtual void setValueImpl(InfoValue val) = 0;

  /// @brief retrieve the primary quality property - must be implemented by
  /// child classes
  [[nodiscard]] virtual InfoQuality getQualityImpl() const;

  /// @brief update the primary quality property - must be implemented by child
  /// classes
  virtual void setQualityImpl(InfoQuality val);

  /// @brief converts the common attributes of the current Information instance
  /// to a string representation
  std::string base_toString() const;

public:
  // constructor
  explicit IInformation(
      const std::optional<Object::DateTime> &recorded_at = std::nullopt,
      bool readonly = false);

  // destructor
  virtual ~IInformation() = default;

  virtual InformationCategory getCategory() const;

  /// @brief retrieve the primary value property as variant
  [[nodiscard]] virtual InfoValue getValue();

  /// @brief update the primary value property from variant
  virtual void setValue(InfoValue val);

  /// @brief retrieve the primary quality property as variant
  [[nodiscard]] virtual InfoQuality getQuality();

  /// @brief update the primary quality property from variant
  virtual void setQuality(InfoQuality val);

  [[nodiscard]] const std::optional<Object::DateTime> &getRecordedAt() const;

  virtual void setRecordedAt(std::optional<Object::DateTime> val);

  [[nodiscard]] const Object::DateTime &getProcessedAt() const;

  void setProcessedAt(const Object::DateTime &val);

  void injectTimeZone(std::chrono::seconds offset, bool daylightSavingTime);

  virtual void setReadonly();

  [[nodiscard]] bool isReadonly() const;

  /// @brief name of the Information type - must be implemented by child classes
  [[nodiscard]] virtual std::string name() const = 0;

  /// @brief converts the current Information instance to a string
  /// representation
  virtual std::string toString() const;
};

} // namespace Object::Information
#endif // C104_OBJECT_INFORMATION_INTERFACE_H
