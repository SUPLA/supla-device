// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_GENERAL_PURPOSE_METER_H_
#define SRC_SUPLA_SENSOR_GENERAL_PURPOSE_METER_H_

#include "general_purpose_channel_base.h"
#include <supla/action_handler.h>

namespace Supla {
namespace Sensor {
class GeneralPurposeMeter : public GeneralPurposeChannelBase,
                            public ActionHandler {
 public:
#pragma pack(push, 1)
  struct GPMMeterSpecificConfig {
    uint8_t counterType = 0;
    uint8_t includeValueAddedInHistory = 0;
    uint8_t fillMissingData = 0;
  };
#pragma pack(pop)

  explicit GeneralPurposeMeter(MeasurementDriver *driver = nullptr,
                               bool addMemoryVariableDriver = true);

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                              bool local) override;
  void fillChannelConfig(void *channelConfig,
                         int *size, uint8_t configType) override;
  void handleAction(int event, int action) override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;
  void onLoadState() override;
  void onSaveState() override;

  // Set counter to a given value
  void setCounter(double newValue);
  // Increment the counter by incrementBy or by valueStep when incrementBy = 0
  void incCounter(double incrementBy = 0);
  // Decrement the counter by decrementBy or by valueStep when decrementBy = 0
  void decCounter(double decrementBy = 0);
  void setValueStep(double newValueStep);
  void setResetToValue(double newResetToValue);

  void setCounterResetSupportFlag(bool support);
  // Enable or disable keeping state in Storage
  void setKeepStateInStorage(bool keep);

  uint8_t getCounterType() const;
  uint8_t getIncludeValueAddedInHistory() const;
  uint8_t getFillMissingData() const;

  void setCounterType(uint8_t counterType, bool local = true);
  void setIncludeValueAddedInHistory(uint8_t includeValueAddedInHistory,
                                     bool local = true);
  void setFillMissingData(uint8_t fillMissingData, bool local = true);

 protected:
  void saveMeterSpecificConfig();
  double valueStep = 1;
  double resetToValue = 0;
  bool isCounterResetSupported = true;
  bool keepStateInStorage = false;

  GPMMeterSpecificConfig meterSpecificConfig = {};
};

};  // namespace Sensor
};  // namespace Supla
#endif  // SRC_SUPLA_SENSOR_GENERAL_PURPOSE_METER_H_
