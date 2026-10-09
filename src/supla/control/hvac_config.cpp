// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include "hvac_config.h"

#include <supla/channels/channel.h>
#ifndef ARDUINO_ARCH_AVR
#include <supla/suplan/suplan_server_identity.h>
#endif

namespace Supla {
namespace Control {
namespace {
HvacConfiguration scalarsFromWire(const TChannelConfig_HVAC &wire) {
  HvacConfiguration result;
  result.AuxThermometerType = wire.AuxThermometerType;
  result.AntiFreezeAndOverheatProtectionEnabled =
      wire.AntiFreezeAndOverheatProtectionEnabled;
  result.AvailableAlgorithms = wire.AvailableAlgorithms;
  result.UsedAlgorithm = wire.UsedAlgorithm;
  result.MinOnTimeS = wire.MinOnTimeS;
  result.MinOffTimeS = wire.MinOffTimeS;
  result.OutputValueOnError = wire.OutputValueOnError;
  result.Subfunction = wire.Subfunction;
  result.TemperatureSetpointChangeSwitchesToManualMode =
      wire.TemperatureSetpointChangeSwitchesToManualMode;
  result.AuxMinMaxSetpointEnabled = wire.AuxMinMaxSetpointEnabled;
  result.UseSeparateHeatCoolOutputs = wire.UseSeparateHeatCoolOutputs;
  result.ParameterFlags = wire.ParameterFlags;
  result.TemperatureControlType = wire.TemperatureControlType;
  result.LocalUILockingCapabilities = wire.LocalUILockingCapabilities;
  result.LocalUILock = wire.LocalUILock;
  result.MinAllowedTemperatureSetpointFromLocalUI =
      wire.MinAllowedTemperatureSetpointFromLocalUI;
  result.MaxAllowedTemperatureSetpointFromLocalUI =
      wire.MaxAllowedTemperatureSetpointFromLocalUI;
  result.Temperatures = wire.Temperatures;
  return result;
}
TChannelConfig_HVAC scalarsToWire(const HvacConfiguration &config) {
  TChannelConfig_HVAC result = {};
  result.AuxThermometerType = config.AuxThermometerType;
  result.AntiFreezeAndOverheatProtectionEnabled =
      config.AntiFreezeAndOverheatProtectionEnabled;
  result.AvailableAlgorithms = config.AvailableAlgorithms;
  result.UsedAlgorithm = config.UsedAlgorithm;
  result.MinOnTimeS = config.MinOnTimeS;
  result.MinOffTimeS = config.MinOffTimeS;
  result.OutputValueOnError = config.OutputValueOnError;
  result.Subfunction = config.Subfunction;
  result.TemperatureSetpointChangeSwitchesToManualMode =
      config.TemperatureSetpointChangeSwitchesToManualMode;
  result.AuxMinMaxSetpointEnabled = config.AuxMinMaxSetpointEnabled;
  result.UseSeparateHeatCoolOutputs = config.UseSeparateHeatCoolOutputs;
  result.ParameterFlags = config.ParameterFlags;
  result.MasterThermostatIsSet =
      config.reference(config.MasterThermostat).kind !=
      ChannelReferenceKind::NONE;
  result.HeatOrColdSourceSwitchIsSet =
      config.reference(config.HeatOrColdSourceSwitch).kind !=
      ChannelReferenceKind::NONE;
  result.PumpSwitchIsSet = config.reference(config.PumpSwitch).kind !=
      ChannelReferenceKind::NONE;
  result.TemperatureControlType = config.TemperatureControlType;
  result.LocalUILockingCapabilities = config.LocalUILockingCapabilities;
  result.LocalUILock = config.LocalUILock;
  result.MinAllowedTemperatureSetpointFromLocalUI =
      config.MinAllowedTemperatureSetpointFromLocalUI;
  result.MaxAllowedTemperatureSetpointFromLocalUI =
      config.MaxAllowedTemperatureSetpointFromLocalUI;
  result.Temperatures = config.Temperatures;
  return result;
}
}  // namespace
ChannelReference HvacConfiguration::reference(
    const HvacReferenceValue &value) const {
  if (referenceNamespace == HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER) {
    return value.local.isSet ? ChannelReference::local(value.local.channelNo)
                             : ChannelReference{};
  }
  if (referenceNamespace == HvacReferenceNamespace::SERVER_CHANNEL_ID) {
    return value.channelId ? ChannelReference::server(value.channelId)
                           : ChannelReference{};
  }
  return ChannelReference{};
}
bool HvacConfiguration::setReference(
    HvacReferenceValue *value, const ChannelReference &ref,
    const Device::ServerIdentity *identity) {
  if (!value || !ref.valid()) return false;
  HvacReferenceValue replacement;
  if (referenceNamespace == HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER) {
    if (ref.kind == ChannelReferenceKind::SERVER_CHANNEL) return false;
    if (ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER) {
      replacement.local = {static_cast<uint8_t>(ref.id), 1, {0, 0}};
    }
  } else if (referenceNamespace == HvacReferenceNamespace::SERVER_CHANNEL_ID) {
    if (ref.kind == ChannelReferenceKind::SERVER_CHANNEL) {
      replacement.channelId = ref.id;
    } else if (ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER) {
#ifndef ARDUINO_ARCH_AVR
      if (!identity || !identity->reverse(ref.id, &replacement.channelId))
        return false;
#else
      (void)identity;
      return false;
#endif
    }
  } else {
    return false;
  }
  *value = replacement;
  return true;
}
bool HvacConfiguration::setLocalReference(
    HvacReferenceValue *value, int16_t no,
    const Device::ServerIdentity *identity) {
  if (no < -1 || no >= 255) return false;
  return setReference(value, no == -1 ? ChannelReference{}
                                     : ChannelReference::local(no), identity);
}
HvacConfiguration HvacConfiguration::fromLegacy(
    const TChannelConfig_HVAC &wire, uint8_t self) {
  auto result = scalarsFromWire(wire);
  HvacReferenceValue *values[] = {
      &result.MainThermometer, &result.AuxThermometer, &result.BinarySensor,
      &result.MasterThermostat, &result.PumpSwitch,
      &result.HeatOrColdSourceSwitch};
  const uint8_t numbers[] = {wire.MainThermometerChannelNo,
      wire.AuxThermometerChannelNo, wire.BinarySensorChannelNo,
      wire.MasterThermostatChannelNo, wire.PumpSwitchChannelNo,
      wire.HeatOrColdSourceSwitchChannelNo};
  const bool set[] = {numbers[0] != self && numbers[0] != 255,
      numbers[1] != self && numbers[1] != 255,
      numbers[2] != self && numbers[2] != 255,
      wire.MasterThermostatIsSet != 0, wire.PumpSwitchIsSet != 0,
      wire.HeatOrColdSourceSwitchIsSet != 0};
  for (int i = 0; i < 6; ++i) {
    if (set[i]) values[i]->local = {numbers[i], 1, {0, 0}};
  }
  return result;
}
HvacConfiguration HvacConfiguration::fromServer(
    const TChannelConfig_HVAC &wire) {
  auto result = scalarsFromWire(wire);
  result.referenceNamespace = HvacReferenceNamespace::SERVER_CHANNEL_ID;
  result.MainThermometer.channelId = wire.MainThermometerChannelId;
  result.AuxThermometer.channelId = wire.AuxThermometerChannelId;
  result.BinarySensor.channelId = wire.BinarySensorChannelId;
  result.MasterThermostat.channelId = wire.MasterThermostatChannelId;
  result.PumpSwitch.channelId = wire.PumpSwitchChannelId;
  result.HeatOrColdSourceSwitch.channelId =
      wire.HeatOrColdSourceSwitchChannelId;
  return result;
}
TChannelConfig_HVAC HvacConfiguration::localWire(
    const Device::ServerIdentity *identity, uint8_t self) const {
  auto wire = scalarsToWire(*this);
  const HvacReferenceValue *values[] = {
      &MainThermometer, &AuxThermometer, &BinarySensor, &MasterThermostat,
      &PumpSwitch, &HeatOrColdSourceSwitch};
  uint8_t numbers[6];
  bool local[6];
  for (int i = 0; i < 6; ++i) {
    const auto ref = reference(*values[i]);
    const auto resolved = resolveChannelReference(ref, identity);
    local[i] = ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER ||
               resolved.channel;
    numbers[i] = ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER
        ? ref.id : resolved.channel ? resolved.channel->getChannelNumber()
                                    : self;
  }
  wire.MainThermometerChannelNo = numbers[0];
  wire.AuxThermometerChannelNo = numbers[1];
  wire.BinarySensorChannelNo = numbers[2];
  wire.MasterThermostatChannelNo = numbers[3];
  wire.PumpSwitchChannelNo = numbers[4];
  wire.HeatOrColdSourceSwitchChannelNo = numbers[5];
  wire.MasterThermostatIsSet = local[3];
  wire.PumpSwitchIsSet = local[4];
  wire.HeatOrColdSourceSwitchIsSet = local[5];
  return wire;
}
bool HvacConfiguration::serverWire(const Device::ServerIdentity *identity,
                                   TChannelConfig_HVAC *wire) const {
  if (!wire) return false;
  *wire = {};
  if (!validReferences()) return false;
  const HvacReferenceValue *values[] = {
      &MainThermometer, &AuxThermometer, &BinarySensor, &MasterThermostat,
      &PumpSwitch, &HeatOrColdSourceSwitch};
  uint32_t ids[6] = {};
  for (int i = 0; i < 6; ++i) {
    const auto ref = reference(*values[i]);
    if (ref.kind == ChannelReferenceKind::SERVER_CHANNEL) {
      ids[i] = ref.id;
    } else if (ref.kind == ChannelReferenceKind::LOCAL_CHANNEL_NUMBER) {
#ifndef ARDUINO_ARCH_AVR
      if (!identity || !identity->reverse(ref.id, &ids[i])) return false;
#else
      (void)identity;
      return false;
#endif
    }
  }
  *wire = scalarsToWire(*this);
  wire->MainThermometerChannelId = ids[0];
  wire->AuxThermometerChannelId = ids[1];
  wire->BinarySensorChannelId = ids[2];
  wire->MasterThermostatChannelId = ids[3];
  wire->PumpSwitchChannelId = ids[4];
  wire->HeatOrColdSourceSwitchChannelId = ids[5];
  return true;
}
bool HvacConfiguration::validReferences() const {
  const HvacReferenceValue *values[] = {
      &MainThermometer, &AuxThermometer, &BinarySensor, &MasterThermostat,
      &PumpSwitch, &HeatOrColdSourceSwitch};
  if (referenceNamespace != HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER &&
      referenceNamespace != HvacReferenceNamespace::SERVER_CHANNEL_ID)
    return false;
  for (const auto *value : values) {
    if (referenceNamespace == HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER) {
      if (value->local.isSet > 1 || value->local.reserved[0] ||
          value->local.reserved[1] ||
          (value->local.isSet && value->local.channelNo == 255) ||
          (!value->local.isSet && value->local.channelNo)) return false;
    } else if (value->channelId > INT32_MAX) {
      return false;
    }
  }
  return true;
}
HvacStoredConfigV2 storeHvacConfig(const HvacConfiguration &config,
                                   uint32_t function) {
  HvacStoredConfigV2 record;
  record.function = function;
  record.referenceNamespace = config.referenceNamespace;
  record.scalars = config;
  record.references[0] = config.MainThermometer;
  record.references[1] = config.AuxThermometer;
  record.references[2] = config.BinarySensor;
  record.references[3] = config.MasterThermostat;
  record.references[4] = config.PumpSwitch;
  record.references[5] = config.HeatOrColdSourceSwitch;
  return record;
}
bool restoreHvacConfig(const HvacStoredConfigV2 &record,
                       HvacConfiguration *config) {
  if (!config || record.version != 2) return false;
  const auto &scope = record.serverNone;
  if (scope.rootEpoch ? !scope.deviceId || !scope.channelId
                      : scope.deviceId || scope.channelId) return false;
  HvacConfiguration result;
  static_cast<HvacScalars &>(result) = record.scalars;
  result.referenceNamespace = record.referenceNamespace;
  result.MainThermometer = record.references[0];
  result.AuxThermometer = record.references[1];
  result.BinarySensor = record.references[2];
  result.MasterThermostat = record.references[3];
  result.PumpSwitch = record.references[4];
  result.HeatOrColdSourceSwitch = record.references[5];
  if (!result.validReferences()) return false;
  *config = result;
  return true;
}
}  // namespace Control
}  // namespace Supla
