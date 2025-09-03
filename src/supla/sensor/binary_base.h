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

#ifndef SRC_SUPLA_SENSOR_BINARY_BASE_H_
#define SRC_SUPLA_SENSOR_BINARY_BASE_H_

#include <supla/channels/binary_sensor_channel.h>
#include <supla/element_with_channel_actions.h>

class SuplaDeviceClass;

namespace Supla {

namespace Sensor {

#pragma pack(push, 1)
struct BinarySensorConfig {
  union {
    uint8_t reserved[32];
    struct {
      uint16_t timeoutDs;  // 1 ds -> 0.1 s; 0 - not used; range 1..36000
      uint16_t filteringTimeMs;  // 0 - not used; > 0 - filtering time
      uint8_t sensitivity;       // 0 - not used; 1..101 -> 0..100 %;
                                     // value 1 is 0% -> off
    };
  };

  BinarySensorConfig() : reserved{} {}
};

#pragma pack(pop)
class BinaryBase : public ElementWithChannelActions {
 public:
  BinaryBase();
  virtual ~BinaryBase();
  virtual bool getValue() = 0;
  void iterateAlways() override;
  Channel *getChannel() override;
  const Channel *getChannel() const override;
  void onLoadConfig(SuplaDeviceClass *) override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *config,
                                              bool local = false) override;

  /**
   * Get the invert logic setting
   *
   * @return true if invert logic is enabled
   */
  bool isServerInvertLogic() const;

  /**
   * Set the invert logic. Parameter is synchronized with the server.
   *
   * @param invertLogic true if invert logic should be enabled
   * @param local use true if change originates locally from the device (i.e.
   *              from config mode)
   *
   * @return true if invert logic was changed
   */
  bool setServerInvertLogic(bool invertLogic, bool local = true);

  /**
   * Get the timeout in deciseconds (1 == 0.1 s). 0 - not used.
   * When timeout is used, sensor should clear it's state after configured
   * time.
   *
   * @return timeout in deciseconds
   */
  uint16_t getTimeoutDs() const;

  /**
   * Set the timeout in deciseconds (1 == 0.1 s). 0 - not used.
   * When timeout is used, sensor should clear it's state after configured
   * time. Parameter is synchronized with the server.
   *
   * @param timeoutDs
   * @param local use true if change originates locally from the device (i.e.
   *              from config mode)
   *
   * @return true if timeout was changed
   */
  bool setTimeoutDs(uint16_t timeoutDs, bool local = true);

  /**
   * Get the filtering time in ms. 0 - not used.
   * Input state changes shorter than this time are ignored.
   *
   * @return filtering time in ms
   */
  uint16_t getFilteringTimeMs() const;

  /**
   * Set the filtering time in ms. 0 - not used.
   * Input state changes shorter than this time are ignored.
   * Parameter is synchronized with the server.
   *
   * @param filteringTimeMs
   * @param local use true if change originates locally from the device (i.e.
   *              from config mode)
   *
   * @return true if filtering time was changed
   */
  bool setFilteringTimeMs(uint16_t filteringTimeMs, bool local = true);

  /**
   * Get the sensitivity. 0 - not used; 1..101 -> 0..100 %
   * value 1 is 0% -> off
   * Actual interpretation depends on the sensor implementation
   *
   * @return sensitivity
   */
  uint8_t getSensitivity() const;

  /**
   * Set the sensitivity. 0 - not used; 1..101 -> 0..100 %
   * value 1 is 0% -> off
   * Parameter is synchronized with the server.
   *
   * @param sensitivity
   * @param local use true if change originates locally from the device (i.e.
   *              from config mode)
   *
   * @return true if sensitivity was changed
   */
  bool setSensitivity(uint8_t sensitivity, bool local = true);

  /**
   * Set the read interval in ms. Sensor will try to read value every
   * interval. Setting to 0 will configure default value 100 ms
   *
   * @param intervalMs
   */
  void setReadIntervalMs(uint32_t intervalMs);

  /**
   * Purge the configuration
   */
  void purgeConfig() override;

  /**
   * Fill the channel config
   *
   * @param channelConfig
   * @param size
   * @param configType
   */
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

 protected:
  void saveConfig();
  void printConfig();
  uint32_t lastReadTime = 0;
  uint32_t readIntervalMs = 100;
  BinarySensorChannel channel;
  BinarySensorConfig config;
};

}  // namespace Sensor
}  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_BINARY_BASE_H_
