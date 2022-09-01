Skip to content
Search or jump to…
Pull requests
Issues
Marketplace
Explore
 
@milanos 
milanos
/
Supla_ATH_sensor
Public
Code
Issues
Pull requests
Actions
Projects
Wiki
Security
Insights
Settings
Supla_ATH_sensor/SuplaDevice/src/supla/sensor/AHT.h
@milanos
milanos Create AHT.h
Latest commit cee499f 15 minutes ago
 History
 1 contributor
105 lines (88 sloc)  2.47 KB

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

#ifndef SRC_SUPLA_SENSOR_AHT_H_
#define SRC_SUPLA_SENSOR_AHT_H_

#include <Adafruit_AHTX0.h>
#include "therm_hygro_meter.h"

namespace Supla {
namespace Sensor {
class AHT : public ThermHygroMeter {
 public:
	AHT(){
    aht.begin();
    delay(100);
    retryCountTemp = 0;
    retryCountHumi = 0;
    lastValidTemp = TEMPERATURE_NOT_AVAILABLE;
    lastValidHumi = HUMIDITY_NOT_AVAILABLE;
  }

  double getTemp() {
    double value = TEMPERATURE_NOT_AVAILABLE;
	aht.getEvent(&humidity, &temp);
    value = temp.temperature;
    if (isnan(value)) {
      value = TEMPERATURE_NOT_AVAILABLE;
    }

    if (value == TEMPERATURE_NOT_AVAILABLE) {
      retryCountTemp++;
      if (retryCountTemp > 3) {
        retryCountTemp = 0;
      } else {
        value = lastValidTemp;
      }
    } else {
      retryCountTemp = 0;
    }
    lastValidTemp = value;

    return value;
  }

  double getHumi() {
    double value = HUMIDITY_NOT_AVAILABLE;
	aht.getEvent(&humidity, &temp);
    value = humidity.relative_humidity;
    if (isnan(value)) {
      value = HUMIDITY_NOT_AVAILABLE;
    }

    if (value == HUMIDITY_NOT_AVAILABLE) {
      retryCountHumi++;
      if (retryCountHumi > 3) {
        retryCountHumi = 0;
      } else {
        value = lastValidHumi;
      }
    } else {
      retryCountHumi = 0;
    }
    lastValidHumi = value;

    return value;
  }

  void iterateAlways() {
    if (lastReadTime + 10000 < millis()) {
      lastReadTime = millis();
      channel.setNewValue(getTemp(), getHumi());
    }
  }

 protected:
  Adafruit_AHTX0 aht;
  double lastValidTemp;
  double lastValidHumi;
  int8_t retryCountTemp;
  int8_t retryCountHumi;
  sensors_event_t temp;
  sensors_event_t humidity;
  
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_AHT_H_
Footer
© 2022 GitHub, Inc.
Footer navigation
Terms
Privacy
Security
Status
Docs
Contact GitHub
Pricing
API
Training
Blog
About
