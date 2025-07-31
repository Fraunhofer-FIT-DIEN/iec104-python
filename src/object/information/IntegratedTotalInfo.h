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
 * @file IntegratedTotalInfo.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace Object::Information
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_M_IT_H
#define C104_OBJECT_INFORMATION_M_IT_H

#include "object/information/IInformation.h"
#include "variadic.h"

namespace Object {
namespace Information {

/**
 * @brief binary counter value with read sequence number, quality and optional
 * recorded_at timestamp
 */
class BinaryCounterInfo : public IInformation {
protected:
  /// @brief counter value
  int32_t counter;
  /// @brief measurement sequence
  LimitedUInt5 sequence;
  /// @brief measurement quality
  BinaryCounterQuality quality;

  /// @brief frozen state
  bool frozen;
  /// @brief counter value
  int32_t counter_frozen;

  [[nodiscard]] InfoValue getValueImpl() const override;

  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;

  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryCounterInfo>
  create(const int32_t counter,
         const std::variant<LimitedUInt5, int32_t> &sequence = LimitedUInt5{0},
         const BinaryCounterQuality quality = BinaryCounterQuality::None,
         const std::optional<DateTime> &recorded_at = std::nullopt) {
    const auto visitor2 = overloaded{[](LimitedUInt5 v) { return v; },
                                     [](int32_t v) { return LimitedUInt5(v); }};
    return std::make_shared<BinaryCounterInfo>(
        counter, std::visit(visitor2, sequence), quality, recorded_at, false);
  }

  BinaryCounterInfo(const int32_t counter, LimitedUInt5 sequence,
                    const BinaryCounterQuality quality,
                    const std::optional<DateTime> &recorded_at,
                    const bool readonly)
      : IInformation(recorded_at, readonly), frozen(false), counter_frozen(0),
        counter(counter), sequence(std::move(sequence)), quality(quality) {}

  InformationCategory getCategory() const override {
    return MONITORING_COUNTER;
  }

  [[nodiscard]] int32_t getCounter() const { return counter; }

  /**
   * Get frozen value and reset freeze-state
   * @return counter-value from freeze timestamp
   */
  [[nodiscard]] int32_t getCounterFrozen() {
    std::lock_guard<std::recursive_mutex> lock(mtx);

    if (frozen) {
      frozen = false;
      return counter_frozen;
    }
    return counter;
  }

  void freeze(bool with_reset) {
    std::lock_guard<std::recursive_mutex> lock(mtx);

    frozen = true;
    counter_frozen = counter;
    if (with_reset) {
      reset();
    }
  }

  void reset() {
    std::lock_guard<std::recursive_mutex> lock(mtx);

    counter = 0;
    try {
      sequence += 1;
    } catch (std::out_of_range &e) {
      sequence = LimitedUInt5(0);
    }
  }

  [[nodiscard]] const LimitedUInt5 &getSequence() const { return sequence; }

  [[nodiscard]] std::string name() const override {
    return "BinaryCounterInfo";
  }

  [[nodiscard]] std::string toString() const override;
};

} // namespace Information
} // namespace Object
#endif // C104_OBJECT_INFORMATION_M_IT_H
