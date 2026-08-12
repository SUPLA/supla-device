// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_MQTT_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_MQTT_H_

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
  void publishChannelSetup(int channelNumber);
  bool started = false;
  bool enterRegisteredAndReady = false;
  esp_mqtt_client_handle_t client = {};
  char *mqttCaCert = nullptr;
  uint32_t lastStatusUpdateSec = 0;
  ConnectionError connectionError = ConnectionError::NONE;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_MQTT_H_
