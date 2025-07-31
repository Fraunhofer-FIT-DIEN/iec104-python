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
 * @file EquipmentProtectionOutputCircuitEvent.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_EP_TF_H
#define C104_OBJECT_INFORMATION_M_EP_TF_H

#include "object/information/IInformation.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief output circuits info with replay_operating_ms, quality and optional
 * recorded_at timestamp
 */
class ProtectionEquipmentOutputCircuitInfo : public IInformation {
protected:
  /// @brief circuits value
  OutputCircuits circuits;
  /// @brief time value
  LimitedUInt16 relay_operating_ms;
  /// @brief circuits quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentOutputCircuitInfo>
  create(const std::variant<OutputCircuits, int32_t> circuits,
         const std::variant<LimitedUInt16, int32_t> &relay_operating_ms =
             LimitedUInt16{0},
         const Quality quality = Quality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor =
        overloaded{[](OutputCircuits v) { return v; },
                   [](int32_t v) { return OutputCircuits(v); }};
    const auto visitor2 =
        overloaded{[](LimitedUInt16 v) { return v; },
                   [](int32_t v) { return LimitedUInt16(v); }};
    return std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        std::visit(visitor, circuits), std::visit(visitor2, relay_operating_ms),
        quality, recorded_at, false);
  }

  ProtectionEquipmentOutputCircuitInfo(
      const OutputCircuits circuits, LimitedUInt16 relay_operating_ms,
      const Quality quality, const std::optional<DateTime> &recorded_at,
      const bool readonly)
      : IInformation(recorded_at, readonly), circuits(circuits),
        relay_operating_ms(std::move(relay_operating_ms)), quality(quality) {}

  InformationCategory getCategory() const override { return MONITORING_EVENT; }

  [[nodiscard]] OutputCircuits getCircuits() const { return circuits; }

  [[nodiscard]] LimitedUInt16 getRelayOperating_ms() const {
    return relay_operating_ms;
  }

  [[nodiscard]] std::string name() const override {
    return "ProtectionCircuitInfo";
  }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_EP_TF_H
