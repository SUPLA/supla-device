// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
      uint8_t alarmMuted;  // 0 - not used, 1 - alarm is muted,
                           // 2 - alarm is not muted
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
   * Enable a one-time local turn action after the initial sensor value has
   * become stable. The default is disabled to preserve the existing startup
   * behavior.
   */
  void setTurnActionSyncOnStartup(bool enabled = true);

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
   * Get the alarm muted state. 0 - not used, 1 - muted, 2 - not muted.
   *
   * @return alarm muted state
   */
  uint8_t getAlarmMuted() const;

  /**
   * Set the alarm muted state. 0 - not used, 1 - muted, 2 - not muted.
   * Parameter is synchronized with the server.
   *
   * @param alarmMuted
   * @param local use true if change originates locally from the device (i.e.
   *              from config mode)
   *
   * @return true if alarm muted state was changed
   */
  bool setAlarmMuted(uint8_t alarmMuted, bool local = true);

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
  void beginInitialChannelValueRead();
  void setInitialChannelValue(bool value);
  void notifyInputStateChangeCandidate();
  void setChannelValueQuietly(bool value);
  void saveConfig();
  void printConfig();
  void printConfig(const TChannelConfig_BinarySensor *serverConfig);

  enum class LocalActionState : uint8_t {
    LoadingConfig,
    Initializing,
    StartupSync,
    Runtime,
  };

  uint32_t lastReadTime = 0;
  uint32_t readIntervalMs = 100;
  uint32_t startupSyncStartTimeMs = 0;
  LocalActionState localActionState = LocalActionState::LoadingConfig;
  bool turnActionSyncOnStartup = false;
  bool initialStateCandidatePending = false;
  BinarySensorChannel channel;
  BinarySensorConfig config;
};

}  // namespace Sensor
}  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_BINARY_BASE_H_
