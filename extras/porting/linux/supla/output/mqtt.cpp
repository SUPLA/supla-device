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

bool Supla::Output::Mqtt::putContent(int payload) {
  client = Supla::LinuxMqttClient::getInstance();
  std::string payloadStr = std::to_string(payload);
  enum MQTTErrors error = client->publish(controlTopic, payloadStr, qos);
  if (error == MQTT_OK) {
    SUPLA_LOG_DEBUG(
        "[MQTT] Sent %s to topic %s", payloadStr.c_str(), controlTopic.c_str());
  } else {
    SUPLA_LOG_ERROR("[MQTT] Not sent %s to topic %s, error: %d",
                    payloadStr.c_str(),
                    controlTopic.c_str(),
                    error);
  }
  return error == MQTT_OK;
}
bool Supla::Output::Mqtt::putContent(const std::string& payload) {
  client = Supla::LinuxMqttClient::getInstance();
  enum MQTTErrors error = client->publish(controlTopic, payload, qos);
  if (error == MQTT_OK) {
    SUPLA_LOG_DEBUG(
        "[MQTT] Sent %s to topic %s", payload.c_str(), controlTopic.c_str());
  } else {
    SUPLA_LOG_ERROR("[MQTT] Not sent %s to topic %s, error: %d",
                    payload.c_str(),
                    controlTopic.c_str(),
                    error);
  }
  return error == MQTT_OK;
}
bool Supla::Output::Mqtt::putContent(const std::vector<int>& payload) {
  return false;
}
bool Supla::Output::Mqtt::putContent(bool payload) {
  client = Supla::LinuxMqttClient::getInstance();
  std::string payloadStr = payload ? "true" : "false";
  enum MQTTErrors error = client->publish(controlTopic, payloadStr, qos);
  if (error == MQTT_OK) {
    SUPLA_LOG_DEBUG(
        "[MQTT] Sent %s to topic %s", payloadStr.c_str(), controlTopic.c_str());
  } else {
    SUPLA_LOG_ERROR("[MQTT] Not sent %s to topic %s, error: %d",
                    payloadStr.c_str(),
                    controlTopic.c_str(),
                    error);
  }
  return error == MQTT_OK;
}
