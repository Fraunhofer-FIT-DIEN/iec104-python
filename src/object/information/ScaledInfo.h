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
 * @file ScaledInfo.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_ME_B_H
#define C104_OBJECT_INFORMATION_M_ME_B_H

#include "object/information/IInformation.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief scaled LimitedInt16 value, quality and optional recorded_at timestamp
 */
class ScaledInfo : public IInformation {
protected:
  /// @brief measurement value
  LimitedInt16 actual;
  /// @brief measurement quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ScaledInfo>
  create(const std::variant<LimitedInt16, int32_t> &actual,
         const Quality quality = Quality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor = overloaded{[](LimitedInt16 v) { return v; },
                                    [](int32_t v) { return LimitedInt16(v); }};
    return std::make_shared<ScaledInfo>(std::visit(visitor, actual), quality,
                                        recorded_at, false);
  }

  ScaledInfo(LimitedInt16 actual, const Quality quality,
             const std::optional<DateTime> &recorded_at, const bool readonly)
      : IInformation(recorded_at, readonly), actual(std::move(actual)),
        quality(quality) {}

  [[nodiscard]] const LimitedInt16 &getActual() const { return actual; }

  [[nodiscard]] std::string name() const override { return "ScaledInfo"; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_ME_B_H
