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

#include <linux_mqtt_client.h>
#include <mqtt_client.h>
#include <supla/time.h>

#include <cstdlib>
#include <memory>
#include <unordered_map>

namespace Supla {

std::shared_ptr<Supla::LinuxMqttClient> Supla::LinuxMqttClient::instance =
    nullptr;

std::unordered_map<std::string, std::string> Supla::LinuxMqttClient::topics;

Supla::LinuxMqttClient::LinuxMqttClient(
    const Supla::LinuxYamlConfig& yamlConfig)
    : port(yamlConfig.getMqttPort()),
      verifyCA(yamlConfig.getMqttVerifyCA()),
      useSSL(yamlConfig.getMqttUseSSL()) {
  char buffer[256];
  host = yamlConfig.getMqttHost(buffer) ? buffer : "";
  if (!yamlConfig.getMqttClientName(buffer)) {
    if (gethostname(buffer, sizeof(buffer)) == 0) {
      clientName = buffer;
    } else {
      clientName = "";
    }
  } else {
    clientName = buffer;
  }
  username = yamlConfig.getMqttUsername(buffer) ? buffer : "";
  password = yamlConfig.getMqttPassword(buffer) ? buffer : "";
  fileCA = yamlConfig.getMqttFileCA(buffer) ? buffer : "";
  SUPLA_LOG_DEBUG("Linux MQTT Client create.");
}

Supla::LinuxMqttClient::~LinuxMqttClient() {
  mqtt_client_free();
}

void Supla::LinuxMqttClient::start() {
  if (instance) {
    SUPLA_LOG_INFO("Init Linux MQTT client daemon.");
    instance->mqttClientInit();
  }
}

std::shared_ptr<LinuxMqttClient>& LinuxMqttClient::getInstance() {
  if (!instance) {
    SUPLA_LOG_ERROR("Not find Linux MQTT client instance.");
  }
  return instance;
}

std::shared_ptr<Supla::LinuxMqttClient>& Supla::LinuxMqttClient::getInstance(
    const Supla::LinuxYamlConfig& yamlConfig) {
  if (!instance) {
    instance.reset(new Supla::LinuxMqttClient(yamlConfig));
  }
  SUPLA_LOG_DEBUG("Get Linux MQTT client instance.");
  return instance;
}

void Supla::LinuxMqttClient::subscribeTopic(const std::string& topic, int qos) {
  topics[topic] = "";
}

void Supla::LinuxMqttClient::unsubscribeTopic(const std::string& topic) {
  topics.erase(topic);
  auto reconnect_state =
      static_cast<reconnect_state_t*>(mq_client->reconnect_state);
  reconnect_state->topics.erase(topic);
  mqtt_unsubscribe(mq_client, topic.c_str());
  SUPLA_LOG_DEBUG("unsubscribing %s", topic.c_str());
}

int LinuxMqttClient::mqttClientInit() {
  SUPLA_LOG_DEBUG("Linux MQTT client init.");
  return mqtt_client_init(
      host, port, username, password, clientName, topics, publishCallback);
}

void LinuxMqttClient::publishCallback(void**,
                                      struct mqtt_response_publish* published) {
  auto* topic_name = reinterpret_cast<const char*>(published->topic_name);
  auto* application_message =
      reinterpret_cast<const char*>(published->application_message);
  std::string topic_name_string(topic_name, published->topic_name_size);
  std::string application_message_string(application_message,
                                         published->application_message_size);

  topics[topic_name_string] = application_message_string;

  SUPLA_LOG_DEBUG("Linux MQTT client received message from %s: %s",
                  topic_name_string.c_str(),
                  application_message_string.c_str());
}
enum MQTTErrors Supla::LinuxMqttClient::publish(const std::string& topic,
                                                const std::string& payload,
                                                int qos = MQTT_PUBLISH_QOS_0) {
  if (mq_client == nullptr) {
    SUPLA_LOG_WARNING("No connection to MQTT broker");
    return MQTTErrors::MQTT_ERROR_NULLPTR;
  }
  if (mq_client->error != MQTTErrors::MQTT_OK) {
    return mq_client->error;
  }
  return mqtt_publish(mq_client,
                      topic.c_str(),
                      static_cast<const void*>(payload.c_str()),
                      payload.size(),
                      qos);
}
}  // namespace Supla
