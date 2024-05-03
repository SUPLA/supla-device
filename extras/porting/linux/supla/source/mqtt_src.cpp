//
// Created by Mariusz on 01.05.2024.
//

#include "mqtt_src.h"

#include <linux_mqtt_client.h>
#include <linux_yaml_config.h>
#include <supla/log_wrapper.h>

namespace Supla::Source {

Mqtt::Mqtt(Supla::LinuxYamlConfig& yamlConfig,
           const std::string& topic,
           int qos)
    : topic(topic), qos(qos) {
  client = Supla::LinuxMqttClient::getInstance(yamlConfig);
  SUPLA_LOG_DEBUG("Mark topic %s to subscribe", topic.c_str());
  client->subscribeTopic(topic, qos);
}

Mqtt::~Mqtt() {
  if (client) {
    SUPLA_LOG_DEBUG("Mark topic %s to unsubscribe", topic.c_str());
    client->unsubscribeTopic(topic);
  }
}

std::string Supla::Source::Mqtt::getContent() {
  if (client->topics.find(topic) != client->topics.end()) {
    latestMessage = client->topics[topic];
    SUPLA_LOG_DEBUG("get latest message %s for %s", latestMessage.c_str(), topic.c_str());
  }
  return latestMessage;
}

}  // namespace Supla::Source
