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
 * @file ScaledCmd.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_C_SE_B_H
#define C104_OBJECT_INFORMATION_C_SE_B_H

#include "object/information/ICommand.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief LimitedInt16 value, select or execute flag, qualifier of set-point
 * command and optional recorded_at timestamp
 */
class ScaledCmd : public ICommand {
protected:
  /// @brief set-point value
  LimitedInt16 target;

  /// @brief set-point qualifier
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<ScaledCmd>
  create(const std::variant<LimitedInt16, int32_t> &target,
         const std::variant<LimitedUInt7, int32_t> &qualifier = LimitedUInt7{0},
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor = overloaded{[](LimitedInt16 v) { return v; },
                                    [](int32_t v) { return LimitedInt16(v); }};
    const auto visitor2 = overloaded{[](LimitedUInt7 v) { return v; },
                                     [](int32_t v) { return LimitedUInt7(v); }};
    return std::make_shared<ScaledCmd>(std::visit(visitor, target), false,
                                       std::visit(visitor2, qualifier),
                                       recorded_at, false);
  }

  ScaledCmd(LimitedInt16 target, const bool select, LimitedUInt7 qualifier,
            const std::optional<DateTime> &recorded_at, const bool readonly)
      : ICommand(select, recorded_at, readonly), target(std::move(target)),
        qualifier(std::move(qualifier)) {}

  [[nodiscard]] const LimitedInt16 &getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] std::string name() const override { return "ScaledCmd"; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_C_SE_B_H
