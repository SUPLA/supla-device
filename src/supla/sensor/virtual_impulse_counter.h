// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_

#include <supla-common/proto.h>
#include <supla/action_handler.h>
#include <supla/channels/impulse_counter_channel.h>
#include <supla/element_with_channel_actions.h>

namespace Supla {

namespace Sensor {
class VirtualImpulseCounter : public ElementWithChannelActions,
                              public ActionHandler {
 public:
  VirtualImpulseCounter();

  Channel *getChannel() override;
  const Channel *getChannel() const override;
  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void purgeConfig() override;
  void onRegistered(Supla::Protocol::SuplaSrpc *srpc) override;
  void handleSetChannelConfigResult(TSDS_SetChannelConfigResult *result)
      override;
  void onLoadState() override;
  void onSaveState() override;
  void iterateAlways() override;
  void handleAction(int event, int action) override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;

  // Returns value of a counter at given Supla channel
  uint64_t getCounter() const;

  // Set counter to a given value
  void setCounter(uint64_t value);

  // Increment the counter by 1
  void incCounter();

  virtual void resetCounter();

  void setForceStateSaveOnChange(bool value);
  void setDefaultImpulsesPerUnit(uint32_t impulsesPerUnit);
  static constexpr int32_t MaxImpulsesPerUnit = 1000000;
  int32_t getImpulsesPerUnit() const;
  bool setImpulsesPerUnit(int32_t impulsesPerUnit);
  void setLocalFunction(uint32_t function);
  const char *getCustomUnit() const;
  bool setCustomUnit(const char *unit);
  double getCalculatedValue(uint64_t count) const;

 protected:
  bool setAndSaveFunction(uint32_t function) override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override;
  void fillChannelConfig(void *channelConfig, int *size, uint8_t type) override;

  ImpulseCounterChannel channel;
  uint64_t counter = 0;  // Actual count of impulses
  uint32_t lastReadTime = 0;
  uint32_t defaultImpulsesPerUnit = 1000;
  bool forceStateSaveOnChange = false;

 private:
  bool storeConfig(const TChannelConfig_ImpulseCounter &config, bool local);

  bool hasCounterConfig = false;
  bool awaitingConfigForFunction = false;
  bool ignorePreviousDefaultReply = false;
  bool counterResetRequested = false;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_
