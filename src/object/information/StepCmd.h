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
 * @file StepCmd.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_C_RC_H
#define C104_OBJECT_INFORMATION_C_RC_H

#include "object/information/ICommand.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief step direction, select or execute flag, qualifier of command and
 * optional recorded_at timestamp
 */
class StepCmd : public ICommand {
protected:
  /// @brief step direction
  StepCommandValue step;

  /// @brief command qualifier
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<StepCmd> create(
      const std::variant<StepCommandValue, int32_t> direction,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor = overloaded{
        [](StepCommandValue v) { return v; },
        [](int32_t v) {
          if (v < 1 || v > 2)
            throw std::out_of_range("Not a valid c104.Step value (1-2)");
          return static_cast<StepCommandValue>(v);
        }};
    return std::make_shared<StepCmd>(std::visit(visitor, direction), false,
                                     qualifier, recorded_at, false);
  }

  StepCmd(const StepCommandValue direction, const bool select,
          const CS101_QualifierOfCommand qualifier,
          const std::optional<DateTime> &recorded_at, const bool readonly)
      : ICommand(select, recorded_at, readonly), step(direction),
        qualifier(qualifier) {}

  [[nodiscard]] StepCommandValue getStep() const { return step; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] std::string name() const override { return "StepCmd"; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_C_RC_H
