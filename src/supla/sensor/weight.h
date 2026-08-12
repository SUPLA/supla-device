// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_WEIGHT_H_
#define SRC_SUPLA_SENSOR_WEIGHT_H_

#include "supla/channel_element.h"
#include "supla/element.h"
#include "../action_handler.h"
#include "../local_action.h"
#include "../actions.h"

#define WEIGHT_NOT_AVAILABLE -1.0

namespace Supla {
namespace Sensor {
class Weight : public ChannelElement, public ActionHandler {
 public:
  Weight();

  void setRefreshIntervalMs(int intervalMs);

  virtual double getValue();
  virtual void tareScales() = 0;

  void handleAction(int event, int action) override;
  void iterateAlways() override;

 protected:
  uint32_t lastReadTime = 0;
  uint16_t refreshIntervalMs = 10000;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_WEIGHT_H_
