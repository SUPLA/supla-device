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

enum class HomeScreenContent {
  HOME_SCREEN_OFF,
  HOME_SCREEN_TEMPERATURE,
  HOME_SCREEN_TEMPERATURE_HUMIDITY,
  HOME_SCREEN_TIME,
  HOME_SCREEN_TIME_DATE,
  HOME_SCREEN_TEMPERATURE_TIME,
  HOME_SCREEN_MAIN_AND_AUX_TEMPERATURE,
  HOME_SCREEN_MODE_OR_TEMPERATURE,
};

namespace Device {

class RemoteDeviceConfig {
 public:
  // Registers config field. Register each field separately (only single bit
  // value are accepted)
  static void RegisterConfigField(uint64_t fieldBit);
  // Configures screen saver available modes. Set all available modes
  // in single call (this method overwrites previous values)
  static void SetHomeScreenContentAvailable(uint64_t allValues);
  static uint64_t GetHomeScreenContentAvailable();
  static enum HomeScreenContent HomeScreenContentBitToEnum(uint64_t fieldBit);
  static uint64_t HomeScreenEnumToBit(enum HomeScreenContent type);
  static uint64_t HomeScreenIntToBit(int mode);

  explicit RemoteDeviceConfig(bool firstDeviceConfigAfterRegistration = false);
  virtual ~RemoteDeviceConfig();

  void processConfig(TSDS_SetDeviceConfig *config);

  uint8_t getResultCode() const;
  bool isEndFlagReceived() const;
  bool isSetDeviceConfigRequired() const;
  // returns false when it failed to build cfg message
  bool fillSetDeviceConfig(TSDS_SetDeviceConfig *config) const;
  void handleSetDeviceConfigResult(TSDS_SetDeviceConfigResult *result);

 private:
  void processStatusLedConfig(uint64_t fieldBit,
                              TDeviceConfig_StatusLed *config);
  void processPowerStatusLedConfig(uint64_t fieldBit,
                              TDeviceConfig_PowerStatusLed *config);
  void processScreenBrightnessConfig(uint64_t fieldBit,
                                    TDeviceConfig_ScreenBrightness *config);
  void processButtonVolumeConfig(uint64_t fieldBit,
                                TDeviceConfig_ButtonVolume *config);
  void processHomeScreenContentConfig(uint64_t fieldBit,
                                   TDeviceConfig_HomeScreenContent *config);
  void processHomeScreenDelayConfig(uint64_t fieldBit,
                                    TDeviceConfig_HomeScreenOffDelay *config);
  void processAutomaticTimeSyncConfig(uint64_t fieldBit,
                                     TDeviceConfig_AutomaticTimeSync *config);
  void processDisableUserInterfaceConfig(
      uint64_t fieldBit, TDeviceConfig_DisableUserInterface *config);
  void processHomeScreenDelayTypeConfig(
      uint64_t fieldBit, TDeviceConfig_HomeScreenOffDelayType *config);

  void fillStatusLedConfig(TDeviceConfig_StatusLed *config) const;
  void fillPowerStatusLedConfig(TDeviceConfig_PowerStatusLed *config) const;
  void fillScreenBrightnessConfig(TDeviceConfig_ScreenBrightness *config) const;
  void fillButtonVolumeConfig(TDeviceConfig_ButtonVolume *config) const;
  void fillHomeScreenContentConfig(
      TDeviceConfig_HomeScreenContent *config) const;
  void fillHomeScreenDelayConfig(
      TDeviceConfig_HomeScreenOffDelay *config) const;
  void fillAutomaticTimeSyncConfig(
      TDeviceConfig_AutomaticTimeSync *config) const;
  void fillDisableUserInterfaceConfig(
      TDeviceConfig_DisableUserInterface *config) const;
  void fillHomeScreenDelayTypeConfig(
      TDeviceConfig_HomeScreenOffDelayType *config) const;

  bool endFlagReceived = false;
  uint8_t resultCode = 255;
  int messageCounter = 0;
  bool firstDeviceConfigAfterRegistration = false;
  uint64_t requireSetDeviceConfigFields = 0;

  static uint64_t fieldBitsUsedByDevice;
  static uint64_t homeScreenContentAvailable;
};

}  // namespace Device
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_REMOTE_DEVICE_CONFIG_H_
