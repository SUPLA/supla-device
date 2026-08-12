// SPDX-FileCopyrightText: malarz
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXAMPLES_DEVICES_NETTIGO_AIR_MONITOR_HECA_H_
#define EXAMPLES_DEVICES_NETTIGO_AIR_MONITOR_HECA_H_

#include <supla/sensor/SHT3x.h>
#include <supla/control/virtual_relay.h>
#include <supla/sensor/virtual_binary.h>

namespace Supla {
namespace Sensor {
class HECA : public Supla::Sensor::SHT3x {
 public:
  explicit HECA(int8_t humOn = 63, int8_t humOff = 60,
                int8_t address = 0x44)
      : SHT3x(address) {
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

#endif  // EXAMPLES_DEVICES_NETTIGO_AIR_MONITOR_HECA_H_

