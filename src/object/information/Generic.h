/**
 * Copyright 2025-2025 Fraunhofer Institute for Applied Information Technology
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
 * @file Generic.h
 * @brief empty info or cmd information object
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_GENERIC_H
#define C104_OBJECT_INFORMATION_GENERIC_H

#include "object/information/IInformation.h"

namespace Object::Information {

class Generic : public IInformation {
protected:
  InformationCategory category;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

public:
  explicit Generic(InformationCategory category,
                   const std::optional<DateTime> &recorded_at = std::nullopt,
                   bool readonly = false);

  InformationCategory getCategory() const override { return category; }

  // allow generic commands instance to handle timestamps
  [[nodiscard]] std::string name() const override { return "Generic"; }

  [[nodiscard]] virtual bool isSelectable() const { return false; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Object::Information
#endif // C104_OBJECT_INFORMATION_GENERIC_H
