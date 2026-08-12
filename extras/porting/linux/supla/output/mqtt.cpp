// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mqtt.h"

#include <supla/log_wrapper.h>

#include <string>
#include <vector>

Supla::Output::Mqtt::Mqtt(const Supla::LinuxYamlConfig& yamlConfig,
                          const char* topic,
                          int qos)
    : controlTopic(topic), qos(qos) {
  client = Supla::LinuxMqttClient::getInstance(yamlConfig);
}

Supla::Output::Mqtt::~Mqtt() {
}

bool Supla::Output::Mqtt::putContent(const std::string& payload) {
  client = Supla::LinuxMqttClient::getInstance();
  enum MQTTErrors error = client->publish(controlTopic, payload, qos);
  if (error == MQTT_OK) {
    SUPLA_LOG_DEBUG("[MQTT] Sent \"%s\" to topic \"%s\"",
                    payload.c_str(),
                    controlTopic.c_str());
  } else {
    SUPLA_LOG_ERROR("[MQTT] Not sent \"%s\" to topic \"%s\", error: \"%d\"",
                    payload.c_str(),
                    controlTopic.c_str(),
                    error);
  }
  return error == MQTT_OK;
}

bool Supla::Output::Mqtt::putContent(int payload) {
  std::string payloadStr = std::to_string(payload);
  return putContent(payloadStr);
}

bool Supla::Output::Mqtt::putContent(const std::vector<int>& payload) {
  (void)payload;
  SUPLA_LOG_WARNING("putContent(int[]) is not supported implemented");
  return false;
}

bool Supla::Output::Mqtt::putContent(bool payload) {
  std::string payloadStr = payload ? "true" : "false";
  return putContent(payloadStr);
}
