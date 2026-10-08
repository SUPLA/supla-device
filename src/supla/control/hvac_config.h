// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_CONTROL_HVAC_CONFIG_H_
#define SRC_SUPLA_CONTROL_HVAC_CONFIG_H_

#include <stddef.h>
#include <supla-common/proto.h>
#include <supla/channels/channel_reference.h>

namespace Supla {
namespace Control {
#pragma pack(push, 1)
struct HvacScalars {
  uint8_t AuxThermometerType = {};
  uint8_t AntiFreezeAndOverheatProtectionEnabled = {};
  uint16_t AvailableAlgorithms = {};
  uint16_t UsedAlgorithm = {};
  uint16_t MinOnTimeS = {};
  uint16_t MinOffTimeS = {};
  int8_t OutputValueOnError = {};
  uint8_t Subfunction = {};
  uint8_t TemperatureSetpointChangeSwitchesToManualMode = {};
  uint8_t AuxMinMaxSetpointEnabled = {};
  uint8_t UseSeparateHeatCoolOutputs = {};
  HvacParameterFlags ParameterFlags = {};
  uint8_t TemperatureControlType = {};
  uint8_t LocalUILockingCapabilities = {};
  uint8_t LocalUILock = {};
  int16_t MinAllowedTemperatureSetpointFromLocalUI = {};
  int16_t MaxAllowedTemperatureSetpointFromLocalUI = {};
  THVACTemperatureCfg Temperatures = {};
};
#pragma pack(pop)

enum class HvacReferenceNamespace : uint8_t {
  LOCAL_CHANNEL_NUMBER = 1,
  SERVER_CHANNEL_ID = 2,
};

union HvacReferenceValue {
  uint32_t channelId = 0;
  struct {
    uint8_t channelNo;
    uint8_t isSet;
    uint8_t reserved[2];
  } local;
};
static_assert(sizeof(HvacReferenceValue) == 4, "HVAC reference size changed");

struct HvacConfiguration : HvacScalars {
  HvacReferenceNamespace referenceNamespace =
      HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER;
  HvacReferenceValue MainThermometer;
  HvacReferenceValue AuxThermometer;
  HvacReferenceValue BinarySensor;
  HvacReferenceValue MasterThermostat;
  HvacReferenceValue PumpSwitch;
  HvacReferenceValue HeatOrColdSourceSwitch;
  ChannelReference reference(const HvacReferenceValue &value) const;
  bool setReference(HvacReferenceValue *value,
                    const ChannelReference &reference,
                    const Device::ServerIdentity *identity = nullptr);
  bool setLocalReference(HvacReferenceValue *value, int16_t channelNo,
                         const Device::ServerIdentity *identity = nullptr);
  static HvacConfiguration fromLegacy(const TChannelConfig_HVAC &wire,
                                      uint8_t self = 0xff);
  static HvacConfiguration fromServer(const TChannelConfig_HVAC &wire);
  TChannelConfig_HVAC localWire(const Device::ServerIdentity *identity,
                                uint8_t self) const;
  bool serverWire(const Device::ServerIdentity *identity,
                  TChannelConfig_HVAC *wire) const;
  bool validReferences() const;
};

#pragma pack(push, 1)
struct HvacStoredConfigV2 {
  uint8_t version = 2;
  HvacReferenceNamespace referenceNamespace =
      HvacReferenceNamespace::LOCAL_CHANNEL_NUMBER;
  uint32_t function = 0;
  HvacScalars scalars;
  HvacReferenceValue references[6] = {};
};
#pragma pack(pop)
static_assert(sizeof(HvacScalars) == 82, "HVAC scalar format changed");
static_assert(sizeof(HvacStoredConfigV2) == 112, "HVAC V2 format changed");
static_assert(alignof(HvacStoredConfigV2) == 1, "HVAC V2 must be packed");
static_assert(offsetof(HvacStoredConfigV2, function) == 2,
              "HVAC V2 function offset changed");
static_assert(offsetof(HvacStoredConfigV2, scalars) == 6,
              "HVAC V2 scalar offset changed");
static_assert(offsetof(HvacStoredConfigV2, references) == 88,
              "HVAC V2 reference offset changed");

HvacStoredConfigV2 storeHvacConfig(const HvacConfiguration &config,
                                   uint32_t function = 0);
bool restoreHvacConfig(const HvacStoredConfigV2 &record,
                       HvacConfiguration *config);
}  // namespace Control
}  // namespace Supla
#endif  // SRC_SUPLA_CONTROL_HVAC_CONFIG_H_
