// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <linux_mqtt_client.h>
#include <mqtt_client.h>
#include <supla/time.h>

#include <cstdlib>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <cstdio>
#include <stdexcept>

// Destroy the client (and stop its thread) before the topic cache and mutex.
std::mutex Supla::LinuxMqttClient::topicsMutex;
std::unordered_map<std::string, Supla::LinuxMqttClient::Message>
    Supla::LinuxMqttClient::topics;

std::shared_ptr<Supla::LinuxMqttClient> Supla::LinuxMqttClient::instance =
    nullptr;

Supla::LinuxMqttClient::LinuxMqttClient(
    const Supla::LinuxYamlConfig& yamlConfig)
    : useSSL(yamlConfig.getMqttClientUseSSL()),
      verifyCA(yamlConfig.getMqttClientVerifyCA()),
      port(yamlConfig.getMqttClientPort()) {
  char buffer[256];
  host = yamlConfig.getMqttClientHost(buffer) ? buffer : "";
  if (!yamlConfig.getMqttClientName(buffer)) {
    if (gethostname(buffer, sizeof(buffer)) == 0) {
      clientName = buffer;
    } else {
      clientName = "";
    }
  } else {
    clientName = buffer;
  }
  username = yamlConfig.getMqttClientUsername(buffer) ? buffer : "";
  password = yamlConfig.getMqttClientPassword(buffer) ? buffer : "";
  fileCA = yamlConfig.getMqttClientFileCA(buffer) ? buffer : "";
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

std::shared_ptr<Supla::LinuxMqttClient>& Supla::LinuxMqttClient::getInstance() {
  if (!instance) {
    SUPLA_LOG_ERROR("Not find Linux MQTT client instance.");
  }
  return instance;
}

std::shared_ptr<Supla::LinuxMqttClient>& Supla::LinuxMqttClient::getInstance(
    const Supla::LinuxYamlConfig& yamlConfig) {
  if (!yamlConfig.isValidMqttConfig()) {
    throw std::invalid_argument("Invalid MQTT configuration");
  }
  if (!instance) {
    instance.reset(new Supla::LinuxMqttClient(yamlConfig));
  }
  SUPLA_LOG_DEBUG("Get Linux MQTT client instance.");
  return instance;
}

void Supla::LinuxMqttClient::subscribeTopic(const std::string& topic) {
  std::lock_guard<std::mutex> lock(topicsMutex);
  topics.try_emplace(topic);
}

bool Supla::LinuxMqttClient::getMessage(const std::string& topic,
                                       std::string& message) const {
  std::lock_guard<std::mutex> lock(topicsMutex);
  const auto found = topics.find(topic);
  if (found == topics.end()) {
    return false;
  }
  message = found->second.content;
  return true;
}

std::vector<Supla::LinuxMqttClient::Message>
Supla::LinuxMqttClient::getMessages(
    const std::vector<std::string>& requestedTopics) const {
  std::lock_guard<std::mutex> lock(topicsMutex);
  std::vector<Message> result;
  result.reserve(requestedTopics.size());
  for (const auto& topic : requestedTopics) {
    const auto it = topics.find(topic);
    result.push_back(it == topics.end() ? Message{} : it->second);
  }
  return result;
}

void Supla::LinuxMqttClient::unsubscribeTopic(const std::string& topic) {
  {
    std::lock_guard<std::mutex> lock(topicsMutex);
    topics.erase(topic);
  }
  if (mq_client == nullptr || mq_client->reconnect_state == nullptr) {
    return;
  }

  auto reconnect_state =
      static_cast<reconnect_state_t*>(mq_client->reconnect_state);
  reconnect_state->topics.erase(topic);
  mqtt_unsubscribe(mq_client, topic.c_str());
  SUPLA_LOG_DEBUG("unsubscribing %s", topic.c_str());
}

int Supla::LinuxMqttClient::mqttClientInit() {
  SUPLA_LOG_DEBUG("Linux MQTT client init.");
  std::unordered_map<std::string, std::string> subscribedTopics;
  {
    std::lock_guard<std::mutex> lock(topicsMutex);
    for (const auto& entry : topics) {
      subscribedTopics.emplace(entry.first, entry.second.content);
    }
  }
  return mqtt_client_init(
      host, port, username, password, clientName,
      subscribedTopics, publishCallback);
}

void Supla::LinuxMqttClient::publishCallback(void**,
                                      struct mqtt_response_publish* published) {
  auto* topic_name = reinterpret_cast<const char*>(published->topic_name);
  auto* application_message =
      reinterpret_cast<const char*>(published->application_message);
  std::string topic_name_string(topic_name, published->topic_name_size);
  std::string application_message_string(application_message,
                                         published->application_message_size);

  {
    std::lock_guard<std::mutex> lock(topicsMutex);
    topics[topic_name_string] = {application_message_string, true,
                                 std::chrono::steady_clock::now()};
  }

  SUPLA_LOG_DEBUG("Linux MQTT client received message from %s",
                  topic_name_string.c_str());
}
bool Supla::LinuxMqttClient::isConnected() const {
  return mq_client != nullptr && mq_client->error == MQTT_OK;
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
