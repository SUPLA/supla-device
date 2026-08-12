// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_THERM_HYGRO_PRESS_METER_H_
#define SRC_SUPLA_SENSOR_THERM_HYGRO_PRESS_METER_H_

#include "therm_hygro_meter.h"

#define PRESSURE_NOT_AVAILABLE -1.0

namespace Supla {
namespace Sensor {
class ThermHygroPressMeter : public ThermHygroMeter {
 public:
  ThermHygroPressMeter();
  virtual ~ThermHygroPressMeter();
  virtual double getPressure();
  void iterateAlways() override;
  bool iterateConnected() override;
  Element &disableChannelState();
  Channel *getSecondaryChannel() override;
  const Channel *getSecondaryChannel() const override;

  // Override local action methods in order to delegate execution to Channel and
  // Secondary Channel
  void addAction(uint16_t action, ActionHandler &client, uint16_t event,
      bool alwaysEnabled = false) override;
  void addAction(uint16_t action, ActionHandler *client, uint16_t event,
      bool alwaysEnabled = false) override;

 protected:
  Channel pressureChannel;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERM_HYGRO_PRESS_METER_H_
