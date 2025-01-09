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
 * @file Information.h
 * @brief abstract protocol information
 *
 * @package iec104-python
 * @namespace object
 *
 * @authors Martin Unkel <martin.unkel@fit.fraunhofer.de>
 *
 */

#ifndef C104_OBJECT_INFORMATION_H
#define C104_OBJECT_INFORMATION_H

#include <mutex>
#include <optional>
#include <variant>

#include <cs101_information_objects.h>

#include "types.h"

namespace Object {

class Information : public std::enable_shared_from_this<Information> {
private:
  std::mutex mtx;

protected:
  /// @brief timestamp of information value generation, optional
  std::optional<std::chrono::system_clock::time_point> recorded_at;

  /// @brief timestamp of last local processing (sending or receiving)
  std::chrono::system_clock::time_point processed_at;

  /// @brief toggle, if modification is allowed or not
  bool readonly;

  /// @brief retrieve the primary value property - must be implemented by child
  /// classes
  [[nodiscard]] virtual InfoValue getValueImpl() const;
  /// @brief update the primary value property - must be implemented by child
  /// classes
  virtual void setValueImpl(InfoValue val);

  /// @brief retrieve the primary quality property - must be implemented by
  /// child classes
  [[nodiscard]] virtual InfoQuality getQualityImpl() const;
  /// @brief update the primary quality property - must be implemented by child
  /// classes
  virtual void setQualityImpl(InfoQuality val);

  /// @brief converts the common attributes of the current Information instance
  /// to a string representation
  std::string base_toString() const;

public:
  // constructor
  explicit Information(std::optional<std::chrono::system_clock::time_point>
                           recorded_at = std::nullopt,
                       bool readonly = false);

  // destructor
  virtual ~Information() = default;

  /// @brief retrieve the primary value property as variant
  [[nodiscard]] virtual InfoValue getValue();
  /// @brief update the primary value property from variant
  virtual void setValue(InfoValue val);

  /// @brief retrieve the primary quality property as variant
  [[nodiscard]] virtual InfoQuality getQuality();
  /// @brief update the primary quality property from variant
  virtual void setQuality(InfoQuality val);

  [[nodiscard]] const std::optional<std::chrono::system_clock::time_point> &
  getRecordedAt() const {
    return recorded_at;
  }

  virtual void
  setRecordedAt(std::optional<std::chrono::system_clock::time_point> val);

  [[nodiscard]] std::chrono::system_clock::time_point getProcessedAt() const {
    return processed_at;
  }

  void setProcessedAt(std::chrono::system_clock::time_point val);

  virtual void setReadonly();
  [[nodiscard]] bool isReadonly() const { return readonly; }

  /// @brief name of the Information type - must be implemented by child classes
  [[nodiscard]] static std::string name() { return "Information"; }

  /// @brief converts the current Information instance to a string
  /// representation
  virtual std::string toString() const;
};

class Command : public Information {
public:
  explicit Command(std::optional<std::chrono::system_clock::time_point>
                       recorded_at = std::nullopt,
                   bool readonly = false);

  [[nodiscard]] virtual bool isSelectable() const { return false; }
  [[nodiscard]] virtual bool isSelect() const { return false; }

  [[nodiscard]] static std::string name() { return "Command"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief bool value, quality and optional recorded_at timestamp
 */
class SingleInfo : public Information {
protected:
  /// @brief single value
  bool on;
  /// @brief single quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<SingleInfo> create(
      const bool on, const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<SingleInfo>(on, quality, recorded_at, false);
  };

  SingleInfo(
      const bool on, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), on(on), quality(quality){};

  [[nodiscard]] bool isOn() const { return on; }

  [[nodiscard]] static std::string name() { return "SingleInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief bool value, select or execute flag, qualifier of command and optional
 * recorded_at timestamp
 */
class SingleCmd : public Command {
protected:
  /// @brief single value
  bool on;
  /// @brief select or execute command
  bool select;
  /// @brief command qualifier
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<SingleCmd> create(
      const bool on,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<SingleCmd>(on, false, qualifier, recorded_at,
                                       false);
  };

  SingleCmd(
      const bool on, const bool select,
      const CS101_QualifierOfCommand qualifier,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), on(on), select(select),
        qualifier(qualifier){};

  [[nodiscard]] bool isOn() const { return on; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] static std::string name() { return "SingleCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief bool value with transition, quality and optional recorded_at timestamp
 */
class DoubleInfo : public Information {
protected:
  /// @brief double value
  DoublePointValue state;
  /// @brief double quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<DoubleInfo> create(
      const DoublePointValue state, const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<DoubleInfo>(state, quality, recorded_at, false);
  };

  DoubleInfo(
      const DoublePointValue state, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), state(state), quality(quality){};

  [[nodiscard]] DoublePointValue getState() const { return state; }

  [[nodiscard]] static std::string name() { return "DoubleInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief bool value with transition, select or execute flag, qualifier of
 * command and optional recorded_at timestamp
 */
class DoubleCmd : public Command {
protected:
  /// @brief double value
  DoublePointValue state;
  /// @brief select or execute command
  bool select;
  /// @brief command qualifier
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<DoubleCmd> create(
      const DoublePointValue state,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<DoubleCmd>(state, false, qualifier, recorded_at,
                                       false);
  };

  DoubleCmd(
      const DoublePointValue state, const bool select,
      const CS101_QualifierOfCommand qualifier,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), state(state), select(select),
        qualifier(qualifier){};

  [[nodiscard]] DoublePointValue getState() const { return state; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] static std::string name() { return "DoubleCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief step position value with transition info, quality and optional
 * recorded_at timestamp
 */
class StepInfo : public Information {
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
  [[nodiscard]] static std::shared_ptr<StepInfo> create(
      const LimitedInt7 position, const bool transient = false,
      const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<StepInfo>(position, transient, quality, recorded_at,
                                      false);
  };

  StepInfo(
      const LimitedInt7 position, const bool transient, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), position(position),
        transient(transient), quality(quality){};

  [[nodiscard]] const LimitedInt7 &getPosition() const { return position; }

  [[nodiscard]] bool isTransient() const { return transient; }

  [[nodiscard]] static std::string name() { return "StepInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief step direction, select or execute flag, qualifier of command and
 * optional recorded_at timestamp
 */
class StepCmd : public Command {
protected:
  /// @brief step direction
  StepCommandValue step;
  /// @brief select or execute command
  bool select;
  /// @brief command qualifier
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<StepCmd> create(
      const StepCommandValue direction,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<StepCmd>(direction, false, qualifier, recorded_at,
                                     false);
  };

  StepCmd(
      const StepCommandValue direction, const bool select,
      const CS101_QualifierOfCommand qualifier,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), step(direction), select(select),
        qualifier(qualifier){};

  [[nodiscard]] StepCommandValue getStep() const { return step; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] static std::string name() { return "StepCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief binary value, quality and optional recorded_at timestamp
 */
class BinaryInfo : public Information {
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
  [[nodiscard]] static std::shared_ptr<BinaryInfo> create(
      const Byte32 blob, const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<BinaryInfo>(blob, quality, recorded_at, false);
  }

  BinaryInfo(
      const Byte32 blob, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), blob(blob), quality(quality){};

  [[nodiscard]] const Byte32 &getBlob() const { return blob; }

  [[nodiscard]] static std::string name() { return "BinaryInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief binary value and optional recorded_at timestamp
 */
class BinaryCmd : public Command {
protected:
  /// @brief binary blob
  Byte32 blob;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryCmd> create(
      const Byte32 blob,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<BinaryCmd>(blob, recorded_at, false);
  };

  BinaryCmd(
      const Byte32 blob,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), blob(blob){};

  [[nodiscard]] const Byte32 &getBlob() const { return blob; }

  [[nodiscard]] static std::string name() { return "BinaryCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief NormalizedFloat value, quality and optional recorded_at timestamp
 */
class NormalizedInfo : public Information {
protected:
  /// @brief measurement value
  NormalizedFloat actual;
  /// @brief measurement quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<NormalizedInfo> create(
      const NormalizedFloat actual, const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<NormalizedInfo>(actual, quality, recorded_at,
                                            false);
  }

  NormalizedInfo(
      const NormalizedFloat actual, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), actual(actual), quality(quality){};

  [[nodiscard]] const NormalizedFloat &getActual() const { return actual; }

  [[nodiscard]] static std::string name() { return "NormalizedInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief NormalizedFloat value, select or execute flag, qualifier of set-point
 * command and optional recorded_at timestamp
 */
class NormalizedCmd : public Command {
protected:
  /// @brief set-point value
  NormalizedFloat target;
  /// @brief select or execute command
  bool select;
  /// @brief set-point qualifier
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<NormalizedCmd> create(
      const NormalizedFloat target,
      const LimitedUInt7 qualifier = LimitedUInt7{0},
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<NormalizedCmd>(target, false, qualifier,
                                           recorded_at, false);
  };

  NormalizedCmd(
      const NormalizedFloat target, const bool select,
      const LimitedUInt7 qualifier,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), target(target), select(select),
        qualifier(qualifier){};

  [[nodiscard]] const NormalizedFloat &getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] static std::string name() { return "NormalizedCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief scaled LimitedInt16 value, quality and optional recorded_at timestamp
 */
class ScaledInfo : public Information {
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
  [[nodiscard]] static std::shared_ptr<ScaledInfo> create(
      const LimitedInt16 actual, const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<ScaledInfo>(actual, quality, recorded_at, false);
  };

  ScaledInfo(
      const LimitedInt16 actual, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), actual(actual), quality(quality){};

  [[nodiscard]] const LimitedInt16 &getActual() const { return actual; }

  [[nodiscard]] static std::string name() { return "ScaledInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief LimitedInt16 value, select or execute flag, qualifier of set-point
 * command and optional recorded_at timestamp
 */
class ScaledCmd : public Command {
protected:
  /// @brief set-point value
  LimitedInt16 target;
  /// @brief select or execute command
  bool select;
  /// @brief set-point qualifier
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<ScaledCmd> create(
      const LimitedInt16 target, const LimitedUInt7 qualifier = LimitedUInt7{0},
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<ScaledCmd>(target, false, qualifier, recorded_at,
                                       false);
  };

  ScaledCmd(
      const LimitedInt16 target, const bool select,
      const LimitedUInt7 qualifier,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), target(target), select(select),
        qualifier(qualifier){};

  [[nodiscard]] const LimitedInt16 &getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] static std::string name() { return "ScaledCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief float value, quality and optional recorded_at timestamp
 */
class ShortInfo : public Information {
protected:
  /// @brief measurement value
  float actual;
  /// @brief measurement quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ShortInfo> create(
      const float actual, const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<ShortInfo>(actual, quality, recorded_at, false);
  };

  ShortInfo(
      const float actual, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), actual(actual), quality(quality){};

  [[nodiscard]] float getActual() const { return actual; }

  [[nodiscard]] static std::string name() { return "ShortInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief float value, select or execute flag, qualifier of set-point command
 * and optional recorded_at timestamp
 */
class ShortCmd : public Command {
protected:
  /// @brief set-point value
  float target;
  /// @brief select or execute command
  bool select;
  /// @brief set-point qualifier
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<ShortCmd> create(
      const float target, const LimitedUInt7 qualifier = LimitedUInt7{0},
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<ShortCmd>(target, false, qualifier, recorded_at,
                                      false);
  };

  ShortCmd(
      const float target, const bool select, const LimitedUInt7 qualifier,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Command(recorded_at, readonly), target(target), select(select),
        qualifier(qualifier){};

  [[nodiscard]] float getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] static std::string name() { return "ShortCmd"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief binary counter value with read sequence number, quality and optional
 * recorded_at timestamp
 */
class BinaryCounterInfo : public Information {
protected:
  /// @brief counter value
  int32_t counter;
  /// @brief measurement sequence
  LimitedUInt5 sequence;
  /// @brief measurement quality
  BinaryCounterQuality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryCounterInfo> create(
      const int32_t counter, const LimitedUInt5 sequence = LimitedUInt5{0},
      const BinaryCounterQuality quality = BinaryCounterQuality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<BinaryCounterInfo>(counter, sequence, quality,
                                               recorded_at, false);
  };

  BinaryCounterInfo(
      const int32_t counter, const LimitedUInt5 sequence,
      const BinaryCounterQuality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), counter(counter),
        sequence(sequence), quality(quality){};

  [[nodiscard]] int32_t getCounter() const { return counter; }

  [[nodiscard]] const LimitedUInt5 &getSequence() const { return sequence; }

  [[nodiscard]] static std::string name() { return "BinaryCounterInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief event state info with elapsed_ms, quality and optional recorded_at
 * timestamp
 */
class ProtectionEquipmentEventInfo : public Information {
protected:
  /// @brief event value
  EventState state;
  /// @brief time value
  LimitedUInt16 elapsed_ms;
  /// @brief event quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentEventInfo> create(
      const EventState state, const LimitedUInt16 elapsed_ms = LimitedUInt16{0},
      const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<ProtectionEquipmentEventInfo>(
        state, elapsed_ms, quality, recorded_at, false);
  };

  ProtectionEquipmentEventInfo(
      const EventState state, const LimitedUInt16 elapsed_ms,
      const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), state(state),
        elapsed_ms(elapsed_ms), quality(quality){};

  [[nodiscard]] EventState getState() const { return state; }

  [[nodiscard]] LimitedUInt16 getElapsed_ms() const { return elapsed_ms; }

  [[nodiscard]] static std::string name() { return "ProtectionEventInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief start events info with relay_duration_ms, quality and optional
 * recorded_at timestamp
 */
class ProtectionEquipmentStartEventsInfo : public Information {
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
  create(const StartEvents events,
         const LimitedUInt16 relay_duration_ms = LimitedUInt16{0},
         const Quality quality = Quality::None,
         const std::optional<std::chrono::system_clock::time_point>
             recorded_at = std::nullopt) {
    return std::make_shared<ProtectionEquipmentStartEventsInfo>(
        events, relay_duration_ms, quality, recorded_at, false);
  };

  ProtectionEquipmentStartEventsInfo(
      const StartEvents events, const LimitedUInt16 relay_duration_ms,
      const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), events(events),
        relay_duration_ms(relay_duration_ms), quality(quality){};

  [[nodiscard]] StartEvents getEvents() const { return events; }

  [[nodiscard]] LimitedUInt16 getRelayDuration_ms() const {
    return relay_duration_ms;
  }

  [[nodiscard]] static std::string name() { return "ProtectionStartInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief output circuits info with replay_operating_ms, quality and optional
 * recorded_at timestamp
 */
class ProtectionEquipmentOutputCircuitInfo : public Information {
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
  create(const OutputCircuits circuits,
         const LimitedUInt16 relay_operating_ms = LimitedUInt16{0},
         const Quality quality = Quality::None,
         const std::optional<std::chrono::system_clock::time_point>
             recorded_at = std::nullopt) {
    return std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        circuits, relay_operating_ms, quality, recorded_at, false);
  };

  ProtectionEquipmentOutputCircuitInfo(
      const OutputCircuits circuits, const LimitedUInt16 relay_operating_ms,
      const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), circuits(circuits),
        relay_operating_ms(relay_operating_ms), quality(quality){};

  [[nodiscard]] OutputCircuits getCircuits() const { return circuits; }

  [[nodiscard]] LimitedUInt16 getRelayOperating_ms() const {
    return relay_operating_ms;
  }

  [[nodiscard]] static std::string name() { return "ProtectionCircuitInfo"; }

  [[nodiscard]] std::string toString() const override;
};

/**
 * @brief 16 packed bool values with change indicator, quality and optional
 * recorded_at timestamp
 */
class StatusWithChangeDetection : public Information {
protected:
  /// @brief status value
  FieldSet16 status;
  /// @brief change detection indication
  FieldSet16 changed;
  /// @brief status quality
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<StatusWithChangeDetection> create(
      const FieldSet16 status, const FieldSet16 changed = FieldSet16{},
      const Quality quality = Quality::None,
      const std::optional<std::chrono::system_clock::time_point> recorded_at =
          std::nullopt) {
    return std::make_shared<StatusWithChangeDetection>(status, changed, quality,
                                                       recorded_at, false);
  };

  StatusWithChangeDetection(
      const FieldSet16 status, const FieldSet16 changed, const Quality quality,
      const std::optional<std::chrono::system_clock::time_point> recorded_at,
      bool readonly)
      : Information(recorded_at, readonly), status(status), changed(changed),
        quality(quality){};

  [[nodiscard]] FieldSet16 getStatus() const { return status; }

  [[nodiscard]] FieldSet16 getChanged() const { return changed; }

  [[nodiscard]] static std::string name() { return "StatusAndChanged"; }

  [[nodiscard]] std::string toString() const override;
};

};     // namespace Object
#endif // C104_OBJECT_INFORMATION_H
