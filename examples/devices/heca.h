/*
   Copyright (C) malarz

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

#ifndef SRC_SUPLA_SENSOR_HECA_H_
#define SRC_SUPLA_SENSOR_HECA_H_

#include <supla/sensor/SHT3x.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/virtual_binary.h>

namespace Supla {
namespace Sensor {
class HECA : public Supla::Sensor::SHT3x {
 public:
  explicit HECA(int8_t humOn = 63, int8_t humOff = 60, int8_t address = 0x44) : SHT3x(address) {
    humiOn = humOn;
    humiOff = humOff;

    channel.setInitialCaption("HECA temp&humi");

    heaterChannel.setDefaultFunction(SUPLA_CHANNELFNC_BINARY_SENSOR);
    heaterChannel.setInitialCaption("HECA heater");
    heaterChannel.getChannel()->setDefaultIcon(2);
    heaterChannel.clear();
  }

  void onInit() override {
    SHT3x::onInit();

    // set hight humidity alert and dummy hight temperature alert
    sht.writeAlertHigh(120, 119, humiOn, humiOff);
    // set dummy low alert
    sht.writeAlertLow(5, -5, 0, 1);
    // reset all registers for proper operation of alerts
    sht.clearAll();
  }

  double getHumi() override {
    // ugly method overriding
    // it should be in readValuesFromDevice() method (it's private)
    // but this method is much shorter

    if (sht.readStatusRegister().HeaterStatus) {
      heaterChannel.set();
      SUPLA_LOG_DEBUG("HECA heater is ON");
    } else {
      heaterChannel.clear();
      SUPLA_LOG_DEBUG("HECA heater is OFF");
    }

    return humidity;
  }

 protected:
  int8_t humiOn = 63;
  int8_t humiOff = 60;
  Supla::Sensor::VirtualBinary heaterChannel;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_HECA_H_
