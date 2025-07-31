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
 * @file Information.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_PS_H
#define C104_OBJECT_INFORMATION_M_PS_H

#include <mutex>
#include <optional>
#include <utility>

#include "object/information/IInformation.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief 16 packed bool values with change indicator, quality and optional
 * recorded_at timestamp
 */
class StatusWithChangeDetection : public IInformation {
protected:
  /// @brief status value
  FieldSet16 status;
  /// @brief change detection indication
  FieldSet16 changed;
  /// @brief status quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<StatusWithChangeDetection>
  create(const std::variant<FieldSet16, int32_t> status,
         const std::variant<FieldSet16, int32_t> changed = FieldSet16{},
         const Quality quality = Quality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor = overloaded{[](FieldSet16 v) { return v; },
                                    [](int32_t v) { return FieldSet16(v); }};
    return std::make_shared<StatusWithChangeDetection>(
        std::visit(visitor, status), std::visit(visitor, changed), quality,
        recorded_at, false);
  }

  StatusWithChangeDetection(const FieldSet16 status, const FieldSet16 changed,
                            const Quality quality,
                            const std::optional<DateTime> &recorded_at,
                            const bool readonly)
      : IInformation(recorded_at, readonly), status(status), changed(changed),
        quality(quality) {}

  InformationCategory getCategory() const override { return MONITORING_STATUS; }

  [[nodiscard]] FieldSet16 getStatus() const { return status; }

  [[nodiscard]] FieldSet16 getChanged() const { return changed; }

  [[nodiscard]] std::string name() const override { return "StatusAndChanged"; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_PS_H
