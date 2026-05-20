/*
   Copyright (C) AC SOFTWARE SP. Z O.O

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_VARILIGHT_DIMMER_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_VARILIGHT_DIMMER_H_

#include <stdint.h>
#include <supla/channel_element.h>

namespace Supla {
namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol

namespace Control {
namespace Varilight {

constexpr uint8_t ConfigVersion = 2;
constexpr int32_t MsgRestoreDefaults = 0x4E;
constexpr int32_t MsgConfigurationMode = 0x44;
constexpr int32_t MsgSetMode = 0x58;
constexpr int32_t MsgSetMinimum = 0x59;
constexpr int32_t MsgSetMaximum = 0x5A;
constexpr int32_t MsgSetBoost = 0x5B;
constexpr int32_t MsgSetBoostLevel = 0x5C;
constexpr int32_t MsgConfigurationAck = 0x45;
constexpr int32_t MsgConfigurationReport = 0x51;
constexpr int32_t MsgConfigComplete = 0x46;
constexpr int32_t CalcfgSetLedConfig = 0x01FF;
constexpr uint8_t DefaultLedConfig = 1;
constexpr int PicHexVersionMaxSize = 20;

#pragma pack(push, 1)
struct Configuration {
  uint16_t edgeMinimum;
  uint16_t edgeMaximum;
  uint16_t operatingMinimum;
  uint16_t operatingMaximum;
  uint8_t mode;
  uint8_t boost;
  uint16_t boostLevel;
  uint8_t childLock;
  uint8_t modeMask;
  uint8_t boostMask;
};

struct SuplaConfiguration {
  Configuration mainConfig;
  uint8_t cfgVersion;
  uint8_t led;
  char picInstalledHexVer[PicHexVersionMaxSize];
};
#pragma pack(pop)

}  // namespace Varilight

class VarilightDimmer : public Supla::ChannelElement {
 public:
  explicit VarilightDimmer(int channelNumber = -1);

  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr) override;
  void iterateAlways() override;
  int32_t handleNewValueFromServer(
      TSD_SuplaChannelNewValue *newValue) override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;
  uint32_t getCalcfgPendingTimeoutMs(
      TSD_DeviceCalCfgRequest *request) const override;

  void setBrightness(uint8_t value);
  uint8_t getBrightness() const;

  void setEdgeMinimum(uint16_t value);
  void setEdgeMaximum(uint16_t value);
  void setOperatingMinimum(uint16_t value);
  void setOperatingMaximum(uint16_t value);
  void setMode(uint8_t value);
  void setBoost(uint8_t value);
  void setBoostLevel(uint16_t value);
  void setChildLock(uint8_t value);
  void setModeMask(uint8_t value);
  void setBoostMask(uint8_t value);
  void setLedConfig(uint8_t value);
  void setPicInstalledHexVersion(const char *value);

  const Varilight::Configuration &getConfiguration() const;
  uint8_t getLedConfig() const;
  Varilight::SuplaConfiguration getSuplaConfiguration() const;

 private:
  static uint16_t clampLevel(uint16_t value);
  static uint8_t clampBrightness(uint8_t value);
  static uint8_t clampLed(uint8_t value);

  void resetToDefaults();
  void startConfiguration();
  Varilight::Configuration &currentConfig();
  const Varilight::Configuration &currentConfig() const;
  void sendConfigurationAck();
  void sendConfigurationReport();
  void completeConfiguration(bool save);

  Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr;
  Varilight::Configuration config = {};
  Varilight::Configuration workingConfig = {};
  uint8_t brightness = 0;
  uint8_t ledConfig = Varilight::DefaultLedConfig;
  uint8_t workingLedConfig = Varilight::DefaultLedConfig;
  char picInstalledHexVer[Varilight::PicHexVersionMaxSize] = {};
  bool configurationActive = false;
  bool configurationAckPending = false;
  bool configurationReportPending = false;
  int32_t configurationReceiverId = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_VARILIGHT_DIMMER_H_
