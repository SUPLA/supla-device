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
  esp_mqtt_event_handle_t event =
      reinterpret_cast<esp_mqtt_event_handle_t>(event_data);
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_CONNECTED");
      break;
    case MQTT_EVENT_DISCONNECTED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_DISCONNECTED");
      break;

    case MQTT_EVENT_SUBSCRIBED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_UNSUBSCRIBED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_PUBLISHED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
      break;
    case MQTT_EVENT_DATA:
      SUPLA_LOG_DEBUG("MQTT_EVENT_DATA");
      break;
    case MQTT_EVENT_ERROR:
      SUPLA_LOG_DEBUG("MQTT_EVENT_ERROR");
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_TCP_TRANSPORT) {
        SUPLA_LOG_DEBUG("Last error code reported from esp-tls: 0x%x",
                        event->error_handle->esp_tls_last_esp_err);
        SUPLA_LOG_DEBUG("Last tls stack error number: 0x%x",
                        event->error_handle->esp_tls_stack_err);
        SUPLA_LOG_DEBUG(
            "Last captured errno : %d (%s)",
            event->error_handle->esp_transport_sock_errno,
            strerror(event->error_handle->esp_transport_sock_errno));
      } else if (event->error_handle->error_type ==
                 MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        SUPLA_LOG_DEBUG("Connection refused error: 0x%x",
                        event->error_handle->connect_return_code);
      } else {
        SUPLA_LOG_DEBUG("Unknown error type: 0x%x",
                        event->error_handle->error_type);
      }
      break;
    default:
      SUPLA_LOG_DEBUG("Other event id:%d", event->event_id);
      break;
  }
}

Supla::Protocol::EspMqtt::EspMqtt(SuplaDeviceClass *sdc)
    : Supla::Protocol::Mqtt(sdc) {
  mutex = Supla::Mutex::Create();
}

void Supla::Protocol::EspMqtt::onInit() {
  if (!isEnabled()) {
    return;
  }

  esp_mqtt_client_config_t mqttCfg = {};
  mqttCfg.broker.address.hostname = server;
  mqttCfg.broker.address.port = port;
  if (useAuth) {
    mqttCfg.credentials.username = user;
    mqttCfg.credentials.authentication.password = password;
  }
  if (useTls) {
    mqttCfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;
  } else {
    mqttCfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  }
  mqttCfg.session.keepalive = 5;

  // TODO(klew): mqttCfg.session.last_will

  client = esp_mqtt_client_init(&mqttCfg);
  esp_mqtt_client_register_event(
      client, MQTT_EVENT_ANY, mqtt_event_handler, nullptr);
}

void Supla::Protocol::EspMqtt::disconnect() {
  if (!isEnabled()) {
    return;
  }

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
  if (!isEnabled()) {
    return;
  }

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
