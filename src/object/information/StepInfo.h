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
 * @file StepInfo.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_ST_H
#define C104_OBJECT_INFORMATION_M_ST_H

#include "object/information/IInformation.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief step position value with transition info, quality and optional
 * recorded_at timestamp
 */
class StepInfo : public IInformation {
protected:
  /// @brief step position value
  LimitedInt7 position;
  /// @brief step is in transition
  bool transient;
  /// @brief step position quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<StepInfo>
  create(const std::variant<LimitedInt7, int32_t> &position,
         const bool transient = false, const Quality quality = Quality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor = overloaded{[](LimitedInt7 v) { return v; },
                                    [](int32_t v) { return LimitedInt7(v); }};
    return std::make_shared<StepInfo>(std::visit(visitor, position), transient,
                                      quality, recorded_at, false);
  }

  StepInfo(LimitedInt7 position, const bool transient, const Quality quality,
           const std::optional<DateTime> &recorded_at, const bool readonly)
      : IInformation(recorded_at, readonly), position(std::move(position)),
        transient(transient), quality(quality) {}

  [[nodiscard]] const LimitedInt7 &getPosition() const { return position; }

  [[nodiscard]] bool isTransient() const { return transient; }

  [[nodiscard]] std::string name() const override { return "StepInfo"; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_ST_H
