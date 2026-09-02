// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mqtt_src.h"

#include <linux_mqtt_client.h>
#include <linux_yaml_config.h>
#include <supla/log_wrapper.h>

#include <vector>
#include <string>

namespace Supla::Source {

Mqtt::Mqtt(const Supla::LinuxYamlConfig& yamlConfig,
           const std::vector<std::string>& topics,
           int qos)
    : topics(topics), qos(qos) {
  client = Supla::LinuxMqttClient::getInstance(yamlConfig);
  for (auto& topic : topics) {
    SUPLA_LOG_DEBUG("Mark topic %s to subscribe", topic.c_str());
    client->subscribeTopic(topic, qos);
  }
}

Mqtt::~Mqtt() {
  if (client) {
    for (auto& topic : topics) {
      SUPLA_LOG_DEBUG("Mark topic %s to unsubscribe", topic.c_str());
      client->unsubscribeTopic(topic);
    }
  }
}

bool Supla::Source::Mqtt::isConnected() {
  return client && client->isConnected();
}

std::string Supla::Source::Mqtt::getContent() {
  std::string combinedMessages;
  for (const auto& topic : topics) {
    if (client->topics.find(topic) != client->topics.end()) {
      std::string currentMessage = client->topics[topic];
      if (combinedMessages.empty()) {
        combinedMessages = currentMessage;
      } else {
        combinedMessages += "\n" + currentMessage;
      }
    }
  }
  latestMessage = combinedMessages;
  return latestMessage;
}

}  // namespace Supla::Source
