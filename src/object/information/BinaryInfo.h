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
 * @file BinaryInfo.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_BO_H
#define C104_OBJECT_INFORMATION_M_BO_H

#include "object/information/IInformation.h"

namespace Object {
namespace Information {

/**
 * @brief binary value, quality and optional recorded_at timestamp
 */
class BinaryInfo : public IInformation {
protected:
  /// @brief binary blob
  Byte32 blob;
  /// @brief binary blob quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryInfo>
  create(const std::variant<Byte32, py::bytes, int32_t> blob,
         const Quality quality = Quality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt);

  BinaryInfo(const Byte32 blob, const Quality quality,
             const std::optional<DateTime> &recorded_at, const bool readonly)
      : IInformation(recorded_at, readonly), blob(blob), quality(quality) {}

  [[nodiscard]] const Byte32 &getBlob() const { return blob; }

  [[nodiscard]] std::string name() const override { return "BinaryInfo"; }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_BO_H
