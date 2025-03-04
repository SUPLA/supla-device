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

#ifndef SRC_SUPLA_SENSOR_CONTAINER_H_
#define SRC_SUPLA_SENSOR_CONTAINER_H_

#include <supla/channel_element.h>

namespace Supla {
namespace Html {
class ContainerParameters;
}  // namespace Html

namespace Sensor {

#pragma pack(push, 1)
struct SensorData {
  uint8_t fillLevel = 0;
  uint8_t channelNumber = 255;  // not used
};

struct ContainerConfig {
  uint8_t warningAboveLevel = 0;  // 0 - not set, 1-101 for 0-100%
  uint8_t alarmAboveLevel = 0;    // 0 - not set, 1-101 for 0-100%
  uint8_t warningBelowLevel = 0;  // 0 - not set, 1-101 for 0-100%
  uint8_t alarmBelowLevel = 0;    // 0 - not set, 1-101 for 0-100%

  uint8_t muteAlarmSoundWithoutAdditionalAuth = 0;  // 0 - admin login is
                                                 // required, 1 - regular
                                                 // user is allowed
  SensorData sensorData[10] = {};
};
#pragma pack(pop)

class Container : public ChannelElement {
 public:
  friend class Supla::Html::ContainerParameters;
  Container();

  /**
   * Sets the internal level reporting flag. When enabled, the container will
   * report fill level by internal measument mechanism. Additional Fill Level
   * Binary Sensors are used only as a backup.
   * Default: false
   *
   * @param internalLevelReporting true to enable internal level reporting
   */
  void setInternalLevelReporting(bool internalLevelReporting);

  /**
   * Checks if the internal level reporting flag is enabled
   *
   * @return true if internal level reporting is enabled
   */
  bool isInternalLevelReporting() const;

  void iterateAlways() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  uint8_t applyChannelConfig(TSD_ChannelConfig *result,
                              bool local = false) override;
  void fillChannelConfig(void *channelConfig, int *size) override;

  void printConfig() const;
  void saveConfig();

  // should return 0-100 value for 0-100 %, -1 if not available
  virtual int readNewValue();

  // 0-100 value for 0-100 %, -1 if not available
  void setValue(int value);

  bool isAlarmActive() const;
  bool isWarningActive() const;
  bool isInvalidSensorStateActive() const;
  bool isSoundAlarmOn() const;

  virtual void setReadIntervalMs(uint32_t timeMs);

  // set warning level as 0-100 value, where
  // -1 - not set, 1-100 for 0-100%
  void setWarningAboveLevel(int8_t warningAboveLevel);
  // returns 0-100 value for 0-100 %, -1 if not available
  int8_t getWarningAboveLevel() const;
  bool isWarningAboveLevelSet() const;

  // set alarm level as 0-100 value, where
  // -1 - not set, 0-100 for 0-100%
  void setAlarmAboveLevel(int8_t alarmAboveLevel);
  // returns 0-100 value for 0-100 %, -1 if not available
  int8_t getAlarmAboveLevel() const;
  bool isAlarmAboveLevelSet() const;

  // set warning level as 0-100 value, where
  // -1 - not set, 1-100 for 0-100%
  void setWarningBelowLevel(int8_t warningBelowLevel);
  // returns 0-100 value for 0-100 %, -1 if not available
  int8_t getWarningBelowLevel() const;
  bool isWarningBelowLevelSet() const;

  // set alarm level as 0-100 value, where
  // -1 - not set, 1-100 for 0-100%
  void setAlarmBelowLevel(int8_t alarmBelowLevel);
  // returns 0-100 value for 0-100 %, -1 if not available
  int8_t getAlarmBelowLevel() const;
  bool isAlarmBelowLevelSet() const;

  void setMuteAlarmSoundWithoutAdditionalAuth(
      bool muteAlarmSoundWithoutAdditionalAuth);
  bool isMuteAlarmSoundWithoutAdditionalAuth() const;

  // returns true when sensor data is set successfully
  // returns false if channel number is not set (i.e. all 10 slots are used)
  bool setSensorData(uint8_t channelNumber, uint8_t fillLevel);
  void removeSensorData(uint8_t channelNumber);

  bool isSensorDataUsed() const;
  bool isAlarmingUsed() const;

  void setSoundAlarmSupported(bool soundAlarmSupported);
  bool isSoundAlarmSupported() const;

  /**
   * Mutes the sound alarmm by clearing channel value flag, but does not
   * clear soundAlarmActivated flag, so it requires to call setSoundAlarmOn with
   * false, before new alarm sound will be started.
   */
  void muteSoundAlarm();

 protected:
  void updateConfigField(uint8_t *configField, int8_t value);
  int8_t getHighestSensorValue() const;
  void setAlarmActive(bool alarmActive);
  void setWarningActive(bool warningActive);
  void setInvalidSensorStateActive(bool invalidSensorStateActive);
  void setSoundAlarmOn(bool soundAlarmOn);

  /**
   * Checks if sensor is in invalid state. Sensor is in invalid state when
   * sensor with lower fill then  the reported fill level, is NOT active
   *
   * @param currentFillLevel current fill level 0..100
   * @param tolerance add tolerance to current fill level so i.e. if fill level
   *        is 92 and tolerance is 2, then sensor is in invalid state when
   *        it is NOT active and it's fill level is 90 or less.
   *
   * @return true if any connected sensor is in invalid state
   */
  bool checkSensorInvalidState(const int8_t currentFillLevel,
                               const int8_t tolerance = 0) const;

  // returns -1 when sensor is not configured or on error
  // returns 0 when sensor is not active
  // returns 1 when sensor is active
  int8_t getSensorState(const uint8_t channelNumber) const;
  uint32_t lastReadTime = 0;
  uint32_t readIntervalMs = 1000;
  int8_t fillLevel = -1;
  bool soundAlarmSupported = false;
  bool soundAlarmActivated = false;

  ContainerConfig config = {};
};

static_assert(sizeof(ContainerConfig().sensorData) / sizeof(SensorData) ==
              sizeof(TChannelConfig_Container().SensorInfo) /
                  sizeof(TContainer_SensorInfo));

};  // namespace Sensor
};  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_CONTAINER_H_
