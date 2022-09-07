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

#include <SuplaDevice.h>
#include <supla/auto_lock.h>
#include <supla/log_wrapper.h>
#include <supla/mutex.h>
#include <supla/storage/config.h>
#include <supla/time.h>

#include "esp_mqtt.h"

Supla::Mutex *Supla::Protocol::EspMqtt::mutex = nullptr;

static void mqtt_event_handler(void *handler_args,
                               esp_event_base_t base,
                               int32_t event_id,
                               void *event_data) {
  // TODO(klew): rewrite
  SUPLA_LOG_DEBUG(" *** MQTT event handler enter");
  Supla::AutoLock autoLock(Supla::Protocol::EspMqtt::mutex);
  SUPLA_LOG_DEBUG(
      " *** MQTT Event dispatched from event loop base=%s, event_id=%d",
      base,
      event_id);
}

Supla::Protocol::EspMqtt::EspMqtt(SuplaDeviceClass *sdc)
    : Supla::Protocol::Mqtt(sdc) {
  mutex = Supla::Mutex::Create();
}

void Supla::Protocol::EspMqtt::onInit() {
  esp_mqtt_client_config_t mqttCfg = {};
  const char uri[] = "mqtt://192.168.8.226";
  mqttCfg.broker.address.uri = uri;
  mqttCfg.credentials.username = "test";
  mqttCfg.credentials.authentication.password = "test";
  mqttCfg.session.keepalive = 5;

  client = esp_mqtt_client_init(&mqttCfg);
  esp_mqtt_client_register_event(
      client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
}

void Supla::Protocol::EspMqtt::disconnect() {
  if (connected) {
    esp_mqtt_client_disconnect(client);
    mutex->unlock();
    SUPLA_LOG_DEBUG("MQTT layer disconnect requested");
    esp_mqtt_client_stop(client);
    connected = false;
  }
}

void Supla::Protocol::EspMqtt::iterate(uint64_t _millis) {
  (void)(_millis);
  if (connected) {
    // this is used to synchronize event loop handling with SuplaDevice.iterate
    mutex->unlock();
    delay(1);
    mutex->lock();
  } else {
    mutex->lock();
    connected = true;
    esp_mqtt_client_start(client);
  }
}
