// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_VALVE_BASE_H_
#define SRC_SUPLA_CONTROL_VALVE_BASE_H_

#include <stddef.h>
#include <supla/channel_element.h>
#include <supla-common/proto.h>
#include <supla/action_handler.h>
#include <supla/channels/channel_reference.h>

namespace Supla {
namespace Control {

#define SUPLA_VALVE_SENSOR_MAX 20

#pragma pack(push, 1)
struct ValveConfig {
  ValveConfig();
  uint8_t sensorData[SUPLA_VALVE_SENSOR_MAX];
  uint8_t closeValveOnFloodType;
  uint8_t reserved[31];
};
using LegacyValveConfig = ValveConfig;
struct ValveConfiguration {
  StoredChannelReference sensorData[SUPLA_VALVE_SENSOR_MAX];
  uint8_t closeValveOnFloodType = 0;
  uint8_t reserved[31] = {};
  bool valid() const;
};
struct ValveStoredConfigV2 {
  uint8_t version = 2;
  uint32_t function = SUPLA_CHANNELFNC_VALVE_OPENCLOSE;
  ValveConfiguration config;
};
static_assert(sizeof(LegacyValveConfig) == 52, "Legacy Valve size");
static_assert(sizeof(ValveStoredConfigV2) == 137, "Valve V2 size");
static_assert(offsetof(ValveStoredConfigV2, function) == 1,
              "V2 function marker offset changed");
static_assert(offsetof(ValveStoredConfigV2, config) == 5,
              "V2 config offset changed");
#pragma pack(pop)

/**
 * ValveBase class is used to handle SUPLA_CHANNEL_TYPE_VALVE_OPENCLOSE and
 * SUPLA_CHANNEL_TYPE_VALVE_PERCENTAGE channels.
 */
class ValveBase : public ChannelElement, public ActionHandler {
 public:
  /**
   * Constructor
   *
   * @param openClose true = openClose valve (two state), false = operates on
   * 0-100 range
   */
  explicit ValveBase(bool openClose = true);
  ~ValveBase() override;
  bool isChannelConfigDurable() const override { return configDurable; }
  void onServerIdentityTransition() override { configDurable = false; }
  void reconcileChannelDependencies() override;
  ChannelReference sensorReference(uint8_t slot) const;

  void onInit() override;
  void iterateAlways() override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void onLoadState() override;
  void onSaveState() override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local = false) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;
  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  void purgeConfig() override;

  /**
   * Implementation of ActionHandler::handleAction.
   * Valve supports following actions:
   * - OPEN
   * - CLOSE
   * - TOGGLE
   *
   * @param event ID of event which triggered action (ignored) (see events.h)
   * @param action ID of action (see actions.h)
   */
  void handleAction(int event, int action) override;

  /**
   * Open valve.
   */
  void openValve();

  /**
   * Close valve.
   */
  void closeValve();

  /**
   * Set valve.
   * OpenClose valve: 0 = closed, >= 1 = open
   *
   * @param openLevel 0-100 (0 = closed, 100 = fully open)
   */
  void setValve(uint8_t openLevel);


  /**
   * Check all sensors and return true if flood is detected
   *
   * @return true if flood is detected
   */
  bool isFloodDetected();

  /**
   * Print current configuration in logs (for debug purpose)
   */
  void printConfig() const;

  /**
   * Save current configuration to Config
   *
   * @param local true = will call setChannelConfig to server
   */
  void saveConfig(bool local = true);

  /**
   * Set value on device or any other interface.
   * Implementation of this method should set the valve level.
   * OpenClose valve: 0 = closed, >= 1 = open
   *
   * @param openLevel 0-100 (0 = closed, 100 = fully open)
   */
  virtual void setValueOnDevice(uint8_t openLevel);

  /**
   * Get value from device or any other interface.
   * Implementation of this method should get the valve level.
   * OpenClose valve: 0 = closed, >= 1 = open
   *
   * @return 0-100 (0 = closed, 100 = fully open)
   */
  virtual uint8_t getValueOpenStateFromDevice();

  /**
   * Add sensor to the channel's configuration. It will trigger setChannelConfig
   * message to server.
   *
   * @param channelNumber channel number of a binary sensor
   *
   * @return true if sensor was added, false otherwise (list is full, channel
   * is not a binary sensor)
   */
  bool addSensor(uint8_t channelNumber);

  /**
   * Remove sensor from the channel's configuration. It will trigger
   * setChannelConfig message to server.
   *
   * @param channelNumber channel number of a binary sensor
   *
   * @return true if sensor was removed, false otherwise (missing on a list)
   */
  bool removeSensor(uint8_t channelNumber);

  /**
   * Set ignore manually opened time in milliseconds.
   * Default value is 30000 (30 seconds).
   * Defines time for how long "manually opened" error check will be ignored
   * since last command was executed (from server, MQTT, etc., local buttons).
   * Time is calculated since last call to openValve(), closeValve(), or
   * setValve().
   *
   * @param timeMs time in milliseconds
   */
  void setIgnoreManuallyOpenedTimeMs(uint32_t timeMs);

  /**
   * Set default close valve on flood type.
   * Default value is SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_NONE
   *
   * @param type SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_*
   */
  void setDefaultCloseValveOnFloodType(uint8_t type);

 protected:
  void onFunctionChange(uint32_t, uint32_t) override {
    // Local UI can stage ChannelFunction before the owning V2 is saved.
    persistenceUncertain = true;
  }
  bool shouldProcessChannelFunctionFromConfig() const override {
    return false;
  }
  bool persistConfig(const ValveConfiguration &candidate, uint32_t function);
  void cleanupLegacy();
  ValveConfiguration config = {};
  uint32_t lastCleanupMs = 0;
  bool cleanupPending = false;
  bool persistenceUncertain = false;
  bool configDurable = true;
  uint32_t lastSensorsCheckTimestamp = 0;
  uint32_t lastUpdateTimestamp = 0;
  uint32_t lastCmdTimestamp = 0;
  uint32_t ignoreManuallyOpenedTimeMs = 30000;
  uint8_t lastOpenLevelState = 0;
  uint8_t defaultCloseValveOnFloodType = SUPLA_VALVE_CLOSE_ON_FLOOD_TYPE_NONE;
  bool previousSensorState[SUPLA_VALVE_SENSOR_MAX] = {};
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_VALVE_BASE_H_
