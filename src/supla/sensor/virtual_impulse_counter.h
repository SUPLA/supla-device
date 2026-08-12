// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_

#include <supla-common/proto.h>
#include <supla/action_handler.h>
#include <supla/channel_element.h>

namespace Supla {

namespace Sensor {
class VirtualImpulseCounter : public ChannelElement, public ActionHandler {
 public:
  VirtualImpulseCounter();

  void onInit() override;
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

 protected:
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

  uint64_t counter = 0;  // Actual count of impulses
  uint32_t lastReadTime = 0;
  uint32_t defaultImpulsesPerUnit = 1000;
  bool forceStateSaveOnChange = false;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_VIRTUAL_IMPULSE_COUNTER_H_
