/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_MQTT_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_MQTT_H_

#include <supla/protocol/mqtt.h>
#include <mqtt_client.h>

namespace Supla {

class Mutex;

namespace Protocol {

class EspMqtt : public Mqtt {
 public:
  explicit EspMqtt(SuplaDeviceClass *sdc);

  void onInit() override;
//  bool onLoadConfig() override;
  void disconnect() override;
  void iterate(uint64_t _millis) override;
 // bool isNetworkRestartRequested() override;
 // uint32_t getConnectionFailTime() override;

  static Supla::Mutex *mutex;
 protected:
  bool connected = false;
  esp_mqtt_client_handle_t client = {};
};

}  // namespace Protocol
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_MQTT_H_
