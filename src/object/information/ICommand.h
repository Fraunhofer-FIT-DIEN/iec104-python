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
 * @file ICommand.h
 * @brief abstract command information object base
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_COMMAND_H
#define C104_OBJECT_INFORMATION_COMMAND_H

#include "object/information/IInformation.h"

namespace Object::Information {

class ICommand : public IInformation {
protected:
  /// @brief select or execute command
  std::atomic_bool select;

public:
  explicit ICommand(bool select = false,
                    const std::optional<DateTime> &recorded_at = std::nullopt,
                    bool readonly = false);

  InformationCategory getCategory() const override { return COMMAND; }

  [[nodiscard]] virtual bool isSelectable() const { return false; }

  [[nodiscard]] bool isSelect() const { return select; }

  void setIsSelect(bool value) { select = value; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Object::Information
#endif // C104_OBJECT_INFORMATION_COMMAND_H
