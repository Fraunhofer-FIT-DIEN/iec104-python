#include "object/Information.h"
#include "types.h" // GetTimestamp_ms

using namespace Object;

Information::Information(const std::optional<uint64_t> recorded_at_ms,
                         const bool readonly)
    : recorded_at_ms(recorded_at_ms), readonly(readonly) {
  processed_at_ms = GetTimestamp_ms();
};

Command::Command(const std::optional<uint64_t> recorded_at_ms,
                 const bool readonly)
    : Information(recorded_at_ms, readonly){};

InfoValue Information::getValueImpl() const { return std::monostate{}; }

void Information::setValueImpl(const InfoValue val){};

InfoQuality Information::getQualityImpl() const { return std::monostate{}; }

void Information::setQualityImpl(const InfoQuality val){};

InfoValue Information::getValue() {
  std::lock_guard<std::mutex> lock(mtx);
  return getValueImpl();
}

void Information::setValue(const InfoValue val) {
  if (readonly) {
    throw std::logic_error("Information is read-only!");
  }

  std::lock_guard<std::mutex> lock(mtx);
  setValueImpl(val);
};

InfoQuality Information::getQuality() {
  std::lock_guard<std::mutex> lock(mtx);
  return getQualityImpl();
}

void Information::setQuality(const InfoQuality val) {
  if (readonly) {
    throw std::logic_error("Information is read-only!");
  }

  std::lock_guard<std::mutex> lock(mtx);
  setQualityImpl(val);
};

void Information::setReadonly() {
  if (readonly) {
    return;
  }
  std::lock_guard<std::mutex> lock(mtx);
  readonly = true;
};

void Information::setRecordedAt_ms(std::optional<uint64_t> val) {
  if (readonly) {
    return;
  }
  std::lock_guard<std::mutex> lock(mtx);
  recorded_at_ms = val;
}

void Information::setProcessedAt_ms(uint64_t timestamp_ms) {
  std::lock_guard<std::mutex> lock(mtx);
  processed_at_ms = timestamp_ms;
}

std::string Information::base_toString() const {
  std::ostringstream oss;
  oss << "recorded_at_ms=";
  if (recorded_at_ms)
    oss << *recorded_at_ms;
  else
    oss << "None";
  oss << ", processed_at_ms=" << processed_at_ms << ", readonly=" << readonly
      << " at " << std::hex << std::showbase
      << reinterpret_cast<std::uintptr_t>(this);
  return oss.str();
}

std::string Information::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " " << Information::base_toString() << ">";
  return oss.str();
}

std::string Command::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " " << Information::base_toString() << ">";
  return oss.str();
}

InfoValue SingleInfo::getValueImpl() const { return on; }

void SingleInfo::setValueImpl(const InfoValue val) { on = std::get<bool>(val); }

InfoQuality SingleInfo::getQualityImpl() const { return quality; }

void SingleInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string SingleInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " on=" << on
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue SingleCmd::getValueImpl() const { return on; }

void SingleCmd::setValueImpl(const InfoValue val) { on = std::get<bool>(val); }

std::string SingleCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " on=" << on
      << ", qualifier=" << QualifierOfCommand_toString(qualifier) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue DoubleInfo::getValueImpl() const { return state; }

void DoubleInfo::setValueImpl(const InfoValue val) {
  state = std::get<DoublePointValue>(val);
}

InfoQuality DoubleInfo::getQualityImpl() const { return quality; }

void DoubleInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string DoubleInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " state=" << DoublePointValue_toString(state)
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue DoubleCmd::getValueImpl() const { return state; }

void DoubleCmd::setValueImpl(const InfoValue val) {
  state = std::get<DoublePointValue>(val);
}

std::string DoubleCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " state=" << DoublePointValue_toString(state)
      << ", qualifier=" << QualifierOfCommand_toString(qualifier) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue StepInfo::getValueImpl() const { return position; }

void StepInfo::setValueImpl(const InfoValue val) {
  position = std::get<LimitedInt7>(val);
}

InfoQuality StepInfo::getQualityImpl() const { return quality; }

void StepInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string StepInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " position=" << position.get()
      << ", transient=" << transient
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue StepCmd::getValueImpl() const { return step; }

void StepCmd::setValueImpl(const InfoValue val) {
  step = std::get<StepCommandValue>(val);
}

std::string StepCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " step=" << StepCommandValue_toString(step)
      << ", qualifier=" << QualifierOfCommand_toString(qualifier) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue BinaryInfo::getValueImpl() const { return blob; }

void BinaryInfo::setValueImpl(const InfoValue val) {
  blob = std::get<Byte32>(val);
}

InfoQuality BinaryInfo::getQualityImpl() const { return quality; }

void BinaryInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string BinaryInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " blob=" << blob.get()
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue BinaryCmd::getValueImpl() const { return blob; }

void BinaryCmd::setValueImpl(const InfoValue val) {
  blob = std::get<Byte32>(val);
}

std::string BinaryCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " blob=" << blob.get() << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue NormalizedInfo::getValueImpl() const { return actual; }

void NormalizedInfo::setValueImpl(const InfoValue val) {
  actual = std::get<NormalizedFloat>(val);
}

InfoQuality NormalizedInfo::getQualityImpl() const { return quality; }

void NormalizedInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string NormalizedInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " actual=" << actual.get()
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue NormalizedCmd::getValueImpl() const { return target; }

void NormalizedCmd::setValueImpl(const InfoValue val) {
  target = std::get<NormalizedFloat>(val);
}

std::string NormalizedCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " target=" << target.get()
      << ", qualifier=" << qualifier.get() << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ScaledInfo::getValueImpl() const { return actual; }

void ScaledInfo::setValueImpl(const InfoValue val) {
  actual = std::get<LimitedInt16>(val);
}

InfoQuality ScaledInfo::getQualityImpl() const { return quality; }

void ScaledInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string ScaledInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " actual=" << actual.get()
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ScaledCmd::getValueImpl() const { return target; }

void ScaledCmd::setValueImpl(const InfoValue val) {
  target = std::get<LimitedInt16>(val);
}

std::string ScaledCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " target=" << target.get()
      << ", qualifier=" << qualifier.get() << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ShortInfo::getValueImpl() const { return actual; }

void ShortInfo::setValueImpl(const InfoValue val) {
  actual = std::get<float>(val);
}

InfoQuality ShortInfo::getQualityImpl() const { return quality; }

void ShortInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string ShortInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " actual=" << actual
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ShortCmd::getValueImpl() const { return target; }

void ShortCmd::setValueImpl(const InfoValue val) {
  target = std::get<float>(val);
}

std::string ShortCmd::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " target=" << target
      << ", qualifier=" << qualifier.get() << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue BinaryCounterInfo::getValueImpl() const { return counter; }

void BinaryCounterInfo::setValueImpl(const InfoValue val) {
  counter = std::get<int32_t>(val);
}

InfoQuality BinaryCounterInfo::getQualityImpl() const { return quality; }

void BinaryCounterInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<BinaryCounterQuality>(val);
}

std::string BinaryCounterInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " counter=" << counter
      << ", sequence=" << sequence.get()
      << ", quality=" << BinaryCounterQuality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ProtectionEquipmentEventInfo::getValueImpl() const { return state; }

void ProtectionEquipmentEventInfo::setValueImpl(const InfoValue val) {
  state = std::get<EventState>(val);
}

InfoQuality ProtectionEquipmentEventInfo::getQualityImpl() const {
  return quality;
}

void ProtectionEquipmentEventInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string ProtectionEquipmentEventInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " state=" << EventState_toString(state)
      << ", elapsed_ms=" << elapsed_ms.get()
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ProtectionEquipmentStartEventsInfo::getValueImpl() const {
  return events;
}

void ProtectionEquipmentStartEventsInfo::setValueImpl(const InfoValue val) {
  events = std::get<StartEvents>(val);
}

InfoQuality ProtectionEquipmentStartEventsInfo::getQualityImpl() const {
  return quality;
}

void ProtectionEquipmentStartEventsInfo::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string ProtectionEquipmentStartEventsInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " events=" << StartEvents_toString(events)
      << ", relay_duration_ms=" << relay_duration_ms.get()
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue ProtectionEquipmentOutputCircuitInfo::getValueImpl() const {
  return circuits;
}

void ProtectionEquipmentOutputCircuitInfo::setValueImpl(const InfoValue val) {
  circuits = std::get<OutputCircuits>(val);
}

InfoQuality ProtectionEquipmentOutputCircuitInfo::getQualityImpl() const {
  return quality;
}

void ProtectionEquipmentOutputCircuitInfo::setQualityImpl(
    const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string ProtectionEquipmentOutputCircuitInfo::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " circuits=" << OutputCircuits_toString(circuits)
      << ", relay_operating_ms=" << relay_operating_ms.get()
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}

InfoValue StatusWithChangeDetection::getValueImpl() const { return status; }

void StatusWithChangeDetection::setValueImpl(const InfoValue val) {
  status = std::get<FieldSet16>(val);
}

InfoQuality StatusWithChangeDetection::getQualityImpl() const {
  return quality;
}

void StatusWithChangeDetection::setQualityImpl(const InfoQuality val) {
  quality = std::get<Quality>(val);
}

std::string StatusWithChangeDetection::toString() const {
  std::ostringstream oss;
  oss << "<c104." << name() << " status=" << FieldSet16_toString(status)
      << ", changed=" << FieldSet16_toString(changed)
      << ", quality=" << Quality_toString(quality) << ", "
      << Information::base_toString() << ">";
  return oss.str();
}
