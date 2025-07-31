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
 * @file EquipmentProtectionStartEvents.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_EP_TE_H
#define C104_OBJECT_INFORMATION_M_EP_TE_H

#include "object/information/IInformation.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief start events info with relay_duration_ms, quality and optional
 * recorded_at timestamp
 */
class ProtectionEquipmentStartEventsInfo : public IInformation {
protected:
  /// @brief events value
  StartEvents events;
  /// @brief time value
  LimitedUInt16 relay_duration_ms;
  /// @brief events quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentStartEventsInfo>
  create(const std::variant<StartEvents, int32_t> events,
         const std::variant<LimitedUInt16, int32_t> &relay_duration_ms =
             LimitedUInt16{0},
         const Quality quality = Quality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor = overloaded{[](StartEvents v) { return v; },
                                    [](int32_t v) { return StartEvents(v); }};
    const auto visitor2 =
        overloaded{[](LimitedUInt16 v) { return v; },
                   [](int32_t v) { return LimitedUInt16(v); }};
    return std::make_shared<ProtectionEquipmentStartEventsInfo>(
        std::visit(visitor, events), std::visit(visitor2, relay_duration_ms),
        quality, recorded_at, false);
  }

  ProtectionEquipmentStartEventsInfo(const StartEvents events,
                                     LimitedUInt16 relay_duration_ms,
                                     const Quality quality,
                                     const std::optional<DateTime> &recorded_at,
                                     const bool readonly)
      : IInformation(recorded_at, readonly), events(events),
        relay_duration_ms(std::move(relay_duration_ms)), quality(quality) {}

  InformationCategory getCategory() const override { return MONITORING_EVENT; }

  [[nodiscard]] StartEvents getEvents() const { return events; }

  [[nodiscard]] LimitedUInt16 getRelayDuration_ms() const {
    return relay_duration_ms;
  }

  [[nodiscard]] std::string name() const override {
    return "ProtectionStartInfo";
  }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_EP_TE_H
