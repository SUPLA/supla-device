/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

  // Override local action methods in order to delegate execution to Channel and
  // Secondary Channel
  void addAction(int action, ActionHandler &client, int event,
      bool alwaysEnabled = false) override;
  void addAction(int action, ActionHandler *client, int event,
      bool alwaysEnabled = false) override;

 protected:
  Channel pressureChannel;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_THERM_HYGRO_PRESS_METER_H_
