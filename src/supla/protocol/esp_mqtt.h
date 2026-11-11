// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PROTOCOL_ESP_MQTT_H_
#define SRC_SUPLA_PROTOCOL_ESP_MQTT_H_

#if defined(ARDUINO_ARCH_ESP32) && __has_include(<esp_arduino_version.h>)
#include <esp_arduino_version.h>
#endif

// Arduino ESP32 core 2.x (ESP-IDF 4.4) has a different esp-mqtt config API
#if defined(SUPLA_DEVICE_ESP32) ||                                \
    (defined(ARDUINO_ARCH_ESP32) && defined(ESP_ARDUINO_VERSION_MAJOR) && \
     ESP_ARDUINO_VERSION_MAJOR >= 3)
#define SUPLA_ESP_MQTT_AVAILABLE

#include <mqtt_client.h>
#include <supla/protocol/mqtt.h>

namespace Supla {

class Mutex;

namespace Protocol {

class EspMqtt : public Mqtt {
 public:
  explicit EspMqtt(SuplaDeviceClass *sdc);
  virtual ~EspMqtt();

  void onInit() override;
  //  bool onLoadConfig() override;
  void disconnect() override;
  bool iterate(uint32_t _millis) override;
  ConnectionError getConnectionError() const override;
  // bool isNetworkRestartRequested() override;
  // uint32_t getConnectionFailTime() override;

  static Supla::Mutex *mutex;
  static Supla::Mutex *mutexEventHandler;
  void setConnecting();
  void setConnectionError(ConnectionError newError);
  void setRegisteredAndReady();

 protected:
  void publishImp(const char *topic,
                          const char *payload,
                          int qos,
                          bool retain) override;
  void subscribeImp(const char *topic, int qos) override;
  void unsubscribeImp(const char *topic) override;
  bool started = false;
  bool enterRegisteredAndReady = false;
  esp_mqtt_client_handle_t client = {};
  char *mqttCaCert = nullptr;
  uint32_t lastStatusUpdateSec = 0;
  ConnectionError connectionError = ConnectionError::NONE;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SUPLA_ESP_MQTT_AVAILABLE

#endif  // SRC_SUPLA_PROTOCOL_ESP_MQTT_H_
