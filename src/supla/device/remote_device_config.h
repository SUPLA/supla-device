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

#ifndef SRC_SUPLA_DEVICE_REMOTE_DEVICE_CONFIG_H_
#define SRC_SUPLA_DEVICE_REMOTE_DEVICE_CONFIG_H_

#include <supla-common/proto.h>
#include <supla/protocol/supla_srpc.h>

namespace Supla {
namespace Device {

const char DeviceConfigChangeCfgTag[] = "devcfg_chng";

class RemoteDeviceConfig {
 public:
  static void RegisterConfigField(uint64_t fieldBit);

  RemoteDeviceConfig();
  virtual ~RemoteDeviceConfig();

  void processConfig(TSDS_SetDeviceConfig *config);

  uint8_t getResultCode() const;
  bool isEndFlagReceived() const;
  bool isSetDeviceConfigRequired() const;
  // returns false when it failed to build cfg message
  bool fillFullSetDeviceConfig(TSDS_SetDeviceConfig *config) const;
  void handleSetDeviceConfigResult(TSDS_SetDeviceConfigResult *result);

 private:
  void processStatusLedConfig(uint64_t fieldBit,
                              TDeviceConfig_StatusLed *config);
  void processScreenBrightnessConfig(uint64_t fieldBit,
                                    TDeviceConfig_ScreenBrightness *config);
  void processTimezoneOffsetConfig(uint64_t fieldBit,
                                  TDeviceConfig_TimezoneOffset *config);
  void processButtonVolumeConfig(uint64_t fieldBit,
                                TDeviceConfig_ButtonVolume *config);
  void processScreensaverModeConfig(uint64_t fieldBit,
                                   TDeviceConfig_ScreensaverMode *config);
  void processScreensaverDelayConfig(uint64_t fieldBit,
                                    TDeviceConfig_ScreensaverDelay *config);
  void processAutomaticTimeSyncConfig(uint64_t fieldBit,
                                     TDeviceConfig_AutomaticTimeSync *config);
  void processDisableLocalConfigConfig(uint64_t fieldBit,
                                      TDeviceConfig_DisableLocalConfig *config);

  void fillStatusLedConfig(TDeviceConfig_StatusLed *config) const;
  void fillScreenBrightnessConfig(TDeviceConfig_ScreenBrightness *config) const;
  void fillTimezoneOffsetConfig(TDeviceConfig_TimezoneOffset *config) const;
  void fillButtonVolumeConfig(TDeviceConfig_ButtonVolume *config) const;
  void fillScreensaverModeConfig(TDeviceConfig_ScreensaverMode *config) const;
  void fillScreensaverDelayConfig(TDeviceConfig_ScreensaverDelay *config) const;
  void fillAutomaticTimeSyncConfig(
      TDeviceConfig_AutomaticTimeSync *config) const;
  void fillDisableLocalConfigConfig(
      TDeviceConfig_DisableLocalConfig *config) const;

  void notifyElementsAboutConfigChange(uint64_t fieldBit) const;

  bool endFlagReceived = false;
  uint8_t resultCode = 255;
  uint64_t fieldBitsFromServer = 0;
  int messageCounter = 0;
  bool requireSetDeviceConfig = false;
  int localConfigChangeFlag = -1;

  static uint64_t fieldBitsUsedByDevice;
};

}  // namespace Device
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_REMOTE_DEVICE_CONFIG_H_
