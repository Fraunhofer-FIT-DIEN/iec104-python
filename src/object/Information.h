//
// Created by unkel on 11.08.2024.
//

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
  std::optional<uint64_t> recorded_at_ms;
  uint64_t processed_at_ms;
  bool readonly;

  [[nodiscard]] virtual InfoValue getValueImpl() const;
  virtual void setValueImpl(InfoValue val);

  [[nodiscard]] virtual InfoQuality getQualityImpl() const;
  virtual void setQualityImpl(InfoQuality val);

  std::string base_toString() const;

  static std::optional<std::uint_fast64_t>
  toTimestamp_ms(const py::object &datetime);

public:
  explicit Information(std::optional<uint64_t> recorded_at_ms = std::nullopt,
                       bool readonly = false);

  [[nodiscard]] virtual InfoValue getValue();
  virtual void setValue(InfoValue val);

  [[nodiscard]] virtual InfoQuality getQuality();
  virtual void setQuality(InfoQuality val);

  [[nodiscard]] const std::optional<uint64_t> &getRecordedAt_ms() const {
    return recorded_at_ms;
  }
  virtual void setRecordedAt_ms(std::optional<uint64_t> val);

  [[nodiscard]] uint64_t getProcessedAt_ms() const { return processed_at_ms; }

  void setProcessedAt_ms(uint64_t timestamp_ms);

  virtual void setReadonly();
  [[nodiscard]] bool isReadonly() const { return readonly; }

  [[nodiscard]] virtual std::string name() const { return "Information"; }

  virtual std::string toString() const;
};

class Command : public Information {
public:
  explicit Command(std::optional<uint64_t> recorded_at_ms = std::nullopt,
                   bool readonly = false);

  [[nodiscard]] virtual bool isSelectable() const { return false; }
  [[nodiscard]] virtual bool isSelect() const { return false; }

  [[nodiscard]] std::string name() const override { return "Command"; }

  std::string toString() const override;
};

class SingleInfo : public Information {
protected:
  bool on;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<SingleInfo>
  create(const bool on, const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<SingleInfo>(on, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<SingleInfo>
  createPy(const bool on, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<SingleInfo>(on, quality,
                                        toTimestamp_ms(recorded_at), false);
  }

  SingleInfo(const bool on, const Quality quality,
             const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), on(on), quality(quality){};

  [[nodiscard]] bool isOn() const { return on; }

  [[nodiscard]] std::string name() const override { return "SingleInfo"; }

  std::string toString() const override;
};

class SingleCmd : public Command {
protected:
  bool on;
  bool select;
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<SingleCmd> create(
      const bool on,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<SingleCmd>(on, false, qualifier, recorded_at_ms,
                                       false);
  }
  [[nodiscard]] static std::shared_ptr<SingleCmd>
  createPy(const bool on, const CS101_QualifierOfCommand qualifier,
           const py::object &recorded_at) {
    return std::make_shared<SingleCmd>(on, false, qualifier,
                                       toTimestamp_ms(recorded_at), false);
  }

  SingleCmd(const bool on, const bool select,
            const CS101_QualifierOfCommand qualifier,
            const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Command(recorded_at_ms, readonly), on(on), select(select),
        qualifier(qualifier){};

  [[nodiscard]] bool isOn() const { return on; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] std::string name() const override { return "SingleCmd"; }

  std::string toString() const override;
};

class DoubleInfo : public Information {
protected:
  DoublePointValue state;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<DoubleInfo>
  create(const DoublePointValue state, const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<DoubleInfo>(state, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<DoubleInfo>
  createPy(const DoublePointValue state, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<DoubleInfo>(state, quality,
                                        toTimestamp_ms(recorded_at), false);
  }

  DoubleInfo(const DoublePointValue state, const Quality quality,
             const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), state(state), quality(quality){};

  [[nodiscard]] DoublePointValue getState() const { return state; }

  [[nodiscard]] std::string name() const override { return "DoubleInfo"; }

  std::string toString() const override;
};

class DoubleCmd : public Command {
protected:
  DoublePointValue state;
  bool select;
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<DoubleCmd> create(
      const DoublePointValue state,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<DoubleCmd>(state, false, qualifier, recorded_at_ms,
                                       false);
  }
  [[nodiscard]] static std::shared_ptr<DoubleCmd>
  createPy(const DoublePointValue state,
           const CS101_QualifierOfCommand qualifier,
           const py::object &recorded_at) {
    return std::make_shared<DoubleCmd>(state, false, qualifier,
                                       toTimestamp_ms(recorded_at), false);
  }

  DoubleCmd(const DoublePointValue state, const bool select,
            const CS101_QualifierOfCommand qualifier,
            const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Command(recorded_at_ms, readonly), state(state), select(select),
        qualifier(qualifier){};

  [[nodiscard]] DoublePointValue getState() const { return state; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] std::string name() const override { return "DoubleCmd"; }

  std::string toString() const override;
};

class StepInfo : public Information {
protected:
  LimitedInt7 position;
  bool transient;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<StepInfo>
  create(const LimitedInt7 position, const bool transient = false,
         const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<StepInfo>(position, transient, quality,
                                      recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<StepInfo>
  createPy(const LimitedInt7 position, const bool transient,
           const Quality quality, const py::object &recorded_at) {
    return std::make_shared<StepInfo>(position, transient, quality,
                                      toTimestamp_ms(recorded_at), false);
  }

  StepInfo(const LimitedInt7 position, const bool transient,
           const Quality quality, const std::optional<uint64_t> recorded_at_ms,
           bool readonly)
      : Information(recorded_at_ms, readonly), position(position),
        transient(transient), quality(quality){};

  [[nodiscard]] const LimitedInt7 &getPosition() const { return position; }

  [[nodiscard]] bool isTransient() const { return transient; }

  [[nodiscard]] std::string name() const override { return "StepInfo"; }

  std::string toString() const override;
};

class StepCmd : public Command {
protected:
  StepCommandValue step;
  bool select;
  CS101_QualifierOfCommand qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<StepCmd> create(
      const StepCommandValue step,
      const CS101_QualifierOfCommand qualifier = CS101_QualifierOfCommand::NONE,
      const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<StepCmd>(step, false, qualifier, recorded_at_ms,
                                     false);
  }
  [[nodiscard]] static std::shared_ptr<StepCmd>
  createPy(const StepCommandValue step,
           const CS101_QualifierOfCommand qualifier,
           const py::object &recorded_at) {
    return std::make_shared<StepCmd>(step, false, qualifier,
                                     toTimestamp_ms(recorded_at), false);
  }

  StepCmd(const StepCommandValue direction, const bool select,
          const CS101_QualifierOfCommand qualifier,
          const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Command(recorded_at_ms, readonly), step(direction), select(select),
        qualifier(qualifier){};

  [[nodiscard]] StepCommandValue getStep() const { return step; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] CS101_QualifierOfCommand getQualifier() const {
    return qualifier;
  }

  [[nodiscard]] std::string name() const override { return "StepCmd"; }

  std::string toString() const override;
};

class BinaryInfo : public Information {
protected:
  Byte32 blob;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryInfo>
  create(const Byte32 blob, const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<BinaryInfo>(blob, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<BinaryInfo>
  createPy(const Byte32 blob, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<BinaryInfo>(blob, quality,
                                        toTimestamp_ms(recorded_at), false);
  }

  BinaryInfo(const Byte32 blob, const Quality quality,
             const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), blob(blob), quality(quality){};

  [[nodiscard]] const Byte32 &getBlob() const { return blob; }

  [[nodiscard]] std::string name() const override { return "BinaryInfo"; }

  std::string toString() const override;
};

class BinaryCmd : public Command {
protected:
  Byte32 blob;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryCmd>
  create(const Byte32 blob,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<BinaryCmd>(blob, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<BinaryCmd>
  createPy(const Byte32 blob, const py::object &recorded_at) {
    return std::make_shared<BinaryCmd>(blob, toTimestamp_ms(recorded_at),
                                       false);
  }

  BinaryCmd(const Byte32 blob, const std::optional<uint64_t> recorded_at_ms,
            bool readonly)
      : Command(recorded_at_ms, readonly), blob(blob){};

  [[nodiscard]] const Byte32 &getBlob() const { return blob; }

  [[nodiscard]] std::string name() const override { return "BinaryCmd"; }

  std::string toString() const override;
};

class NormalizedInfo : public Information {
protected:
  NormalizedFloat actual;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<NormalizedInfo>
  create(const NormalizedFloat actual, const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<NormalizedInfo>(actual, quality, recorded_at_ms,
                                            false);
  }
  [[nodiscard]] static std::shared_ptr<NormalizedInfo>
  createPy(const NormalizedFloat actual, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<NormalizedInfo>(actual, quality,
                                            toTimestamp_ms(recorded_at), false);
  }

  NormalizedInfo(const NormalizedFloat actual, const Quality quality,
                 const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), actual(actual),
        quality(quality){};

  [[nodiscard]] const NormalizedFloat &getActual() const { return actual; }

  [[nodiscard]] std::string name() const override { return "NormalizedInfo"; }

  std::string toString() const override;
};

class NormalizedCmd : public Command {
protected:
  NormalizedFloat target;
  bool select;
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<NormalizedCmd>
  create(const NormalizedFloat target,
         const LimitedUInt7 qualifier = LimitedUInt7{(uint32_t)0},
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<NormalizedCmd>(target, false, qualifier,
                                           recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<NormalizedCmd>
  createPy(const NormalizedFloat target, const LimitedUInt7 qualifier,
           const py::object &recorded_at) {
    return std::make_shared<NormalizedCmd>(target, false, qualifier,
                                           toTimestamp_ms(recorded_at), false);
  }

  NormalizedCmd(const NormalizedFloat target, const bool select,
                const LimitedUInt7 qualifier,
                const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Command(recorded_at_ms, readonly), target(target), select(select),
        qualifier(qualifier){};

  [[nodiscard]] const NormalizedFloat &getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] std::string name() const override { return "NormalizedCmd"; }

  std::string toString() const override;
};

class ScaledInfo : public Information {
protected:
  LimitedInt16 actual;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ScaledInfo>
  create(const LimitedInt16 actual, const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ScaledInfo>(actual, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<ScaledInfo>
  createPy(const LimitedInt16 actual, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<ScaledInfo>(actual, quality,
                                        toTimestamp_ms(recorded_at), false);
  }

  ScaledInfo(const LimitedInt16 actual, const Quality quality,
             const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), actual(actual),
        quality(quality){};

  [[nodiscard]] const LimitedInt16 &getActual() const { return actual; }

  [[nodiscard]] std::string name() const override { return "ScaledInfo"; }

  std::string toString() const override;
};

class ScaledCmd : public Command {
protected:
  LimitedInt16 target;
  bool select;
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<ScaledCmd>
  create(const LimitedInt16 target,
         const LimitedUInt7 qualifier = LimitedUInt7{(uint32_t)0},
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ScaledCmd>(target, false, qualifier, recorded_at_ms,
                                       false);
  }
  [[nodiscard]] static std::shared_ptr<ScaledCmd>
  createPy(const LimitedInt16 target, const LimitedUInt7 qualifier,
           const py::object &recorded_at) {
    return std::make_shared<ScaledCmd>(target, false, qualifier,
                                       toTimestamp_ms(recorded_at), false);
  }

  ScaledCmd(const LimitedInt16 target, const bool select,
            const LimitedUInt7 qualifier,
            const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Command(recorded_at_ms, readonly), target(target), select(select),
        qualifier(qualifier){};

  [[nodiscard]] const LimitedInt16 &getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] std::string name() const override { return "ScaledCmd"; }

  std::string toString() const override;
};

class ShortInfo : public Information {
protected:
  float actual;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ShortInfo>
  create(const float actual, const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ShortInfo>(actual, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<ShortInfo>
  createPy(const float actual, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<ShortInfo>(actual, quality,
                                       toTimestamp_ms(recorded_at), false);
  }

  ShortInfo(const float actual, const Quality quality,
            const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), actual(actual),
        quality(quality){};

  [[nodiscard]] float getActual() const { return actual; }

  [[nodiscard]] std::string name() const override { return "ShortInfo"; }

  std::string toString() const override;
};

class ShortCmd : public Command {
protected:
  float target;
  bool select;
  LimitedUInt7 qualifier;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

public:
  [[nodiscard]] static std::shared_ptr<ShortCmd>
  create(const float target,
         const LimitedUInt7 qualifier = LimitedUInt7{(uint32_t)0},
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ShortCmd>(target, false, qualifier, recorded_at_ms,
                                      false);
  }
  [[nodiscard]] static std::shared_ptr<ShortCmd>
  createPy(const float target, const LimitedUInt7 qualifier,
           const py::object &recorded_at) {
    return std::make_shared<ShortCmd>(target, false, qualifier,
                                      toTimestamp_ms(recorded_at), false);
  }

  ShortCmd(const float target, const bool select, const LimitedUInt7 qualifier,
           const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Command(recorded_at_ms, readonly), target(target), select(select),
        qualifier(qualifier){};

  [[nodiscard]] float getTarget() const { return target; }

  [[nodiscard]] bool isSelectable() const override { return true; }

  [[nodiscard]] bool isSelect() const override { return select; }

  [[nodiscard]] const LimitedUInt7 &getQualifier() const { return qualifier; }

  [[nodiscard]] std::string name() const override { return "ShortCmd"; }

  std::string toString() const override;
};

class BinaryCounterInfo : public Information {
protected:
  int32_t counter;
  LimitedUInt5 sequence;
  BinaryCounterQuality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<BinaryCounterInfo>
  create(const int32_t counter,
         const LimitedUInt5 sequence = LimitedUInt5{(uint32_t)0},
         const BinaryCounterQuality quality = BinaryCounterQuality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<BinaryCounterInfo>(counter, sequence, quality,
                                               recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<BinaryCounterInfo>
  createPy(const int32_t counter, const LimitedUInt5 sequence,
           const BinaryCounterQuality quality, const py::object &recorded_at) {
    return std::make_shared<BinaryCounterInfo>(
        counter, sequence, quality, toTimestamp_ms(recorded_at), false);
  }

  BinaryCounterInfo(const int32_t counter, const LimitedUInt5 sequence,
                    const BinaryCounterQuality quality,
                    const std::optional<uint64_t> recorded_at_ms, bool readonly)
      : Information(recorded_at_ms, readonly), counter(counter),
        sequence(sequence), quality(quality){};

  [[nodiscard]] int32_t getCounter() const { return counter; }

  [[nodiscard]] const LimitedUInt5 &getSequence() const { return sequence; }

  [[nodiscard]] std::string name() const override {
    return "BinaryCounterInfo";
  }

  std::string toString() const override;
};

class ProtectionEquipmentEventInfo : public Information {
protected:
  EventState state;
  LimitedUInt16 elapsed_ms;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentEventInfo>
  create(const EventState state,
         const LimitedUInt16 elapsed_ms = LimitedUInt16{0},
         const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ProtectionEquipmentEventInfo>(
        state, elapsed_ms, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentEventInfo>
  createPy(const EventState state, const LimitedUInt16 elapsed_ms,
           const Quality quality, const py::object &recorded_at) {
    return std::make_shared<ProtectionEquipmentEventInfo>(
        state, elapsed_ms, quality, toTimestamp_ms(recorded_at), false);
  }

  ProtectionEquipmentEventInfo(const EventState state,
                               const LimitedUInt16 elapsed_ms,
                               const Quality quality,
                               const std::optional<uint64_t> recorded_at_ms,
                               bool readonly)
      : Information(recorded_at_ms, readonly), state(state),
        elapsed_ms(elapsed_ms), quality(quality){};

  [[nodiscard]] EventState getState() const { return state; }

  [[nodiscard]] LimitedUInt16 getElapsed_ms() const { return elapsed_ms; }

  [[nodiscard]] std::string name() const override {
    return "ProtectionEventInfo";
  }

  std::string toString() const override;
};

class ProtectionEquipmentStartEventsInfo : public Information {
protected:
  StartEvents events;
  LimitedUInt16 relay_duration_ms;
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
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ProtectionEquipmentStartEventsInfo>(
        events, relay_duration_ms, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentStartEventsInfo>
  createPy(const StartEvents events, const LimitedUInt16 relay_duration_ms,
           const Quality quality, const py::object &recorded_at) {
    return std::make_shared<ProtectionEquipmentStartEventsInfo>(
        events, relay_duration_ms, quality, toTimestamp_ms(recorded_at), false);
  }

  ProtectionEquipmentStartEventsInfo(
      const StartEvents events, const LimitedUInt16 relay_duration_ms,
      const Quality quality, const std::optional<uint64_t> recorded_at_ms,
      bool readonly)
      : Information(recorded_at_ms, readonly), events(events),
        relay_duration_ms(relay_duration_ms), quality(quality){};

  [[nodiscard]] StartEvents getEvents() const { return events; }

  [[nodiscard]] LimitedUInt16 getRelayDuration_ms() const {
    return relay_duration_ms;
  }

  [[nodiscard]] std::string name() const override {
    return "ProtectionStartInfo";
  }

  std::string toString() const override;
};

class ProtectionEquipmentOutputCircuitInfo : public Information {
protected:
  OutputCircuits circuits;
  LimitedUInt16 relay_operating_ms;
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
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        circuits, relay_operating_ms, quality, recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<ProtectionEquipmentOutputCircuitInfo>
  createPy(const OutputCircuits circuits,
           const LimitedUInt16 relay_operating_ms, const Quality quality,
           const py::object &recorded_at) {
    return std::make_shared<ProtectionEquipmentOutputCircuitInfo>(
        circuits, relay_operating_ms, quality, toTimestamp_ms(recorded_at),
        false);
  }

  ProtectionEquipmentOutputCircuitInfo(
      const OutputCircuits circuits, const LimitedUInt16 relay_operating_ms,
      const Quality quality, const std::optional<uint64_t> recorded_at_ms,
      bool readonly)
      : Information(recorded_at_ms, readonly), circuits(circuits),
        relay_operating_ms(relay_operating_ms), quality(quality){};

  [[nodiscard]] OutputCircuits getCircuits() const { return circuits; }

  [[nodiscard]] LimitedUInt16 getRelayOperating_ms() const {
    return relay_operating_ms;
  }

  [[nodiscard]] std::string name() const override {
    return "ProtectionCircuitInfo";
  }

  std::string toString() const override;
};

class StatusWithChangeDetection : public Information {
protected:
  FieldSet16 status;
  FieldSet16 changed;
  Quality quality;

  [[nodiscard]] InfoValue getValueImpl() const override;
  void setValueImpl(InfoValue val) override;

  [[nodiscard]] InfoQuality getQualityImpl() const override;
  void setQualityImpl(InfoQuality val) override;

public:
  [[nodiscard]] static std::shared_ptr<StatusWithChangeDetection>
  create(const FieldSet16 status, const FieldSet16 changed = FieldSet16{},
         const Quality quality = Quality::None,
         const std::optional<uint64_t> recorded_at_ms = std::nullopt) {
    return std::make_shared<StatusWithChangeDetection>(status, changed, quality,
                                                       recorded_at_ms, false);
  }
  [[nodiscard]] static std::shared_ptr<StatusWithChangeDetection>
  createPy(const FieldSet16 status, const FieldSet16 changed,
           const Quality quality, const py::object &recorded_at) {
    return std::make_shared<StatusWithChangeDetection>(
        status, changed, quality, toTimestamp_ms(recorded_at), false);
  }

  StatusWithChangeDetection(const FieldSet16 status, const FieldSet16 changed,
                            const Quality quality,
                            const std::optional<uint64_t> recorded_at_ms,
                            bool readonly)
      : Information(recorded_at_ms, readonly), status(status), changed(changed),
        quality(quality){};

  [[nodiscard]] FieldSet16 getStatus() const { return status; }

  [[nodiscard]] FieldSet16 getChanged() const { return changed; }

  [[nodiscard]] std::string name() const override { return "StatusAndChange"; }

  std::string toString() const override;
};

};     // namespace Object
#endif // C104_OBJECT_INFORMATION_H
