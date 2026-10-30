// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mqtt_src.h"

#include <linux_mqtt_client.h>
#include <linux_yaml_config.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <vector>
#include <stdexcept>
#include <set>
#include <string>

namespace Supla::Source {

Mqtt::Mqtt(const Supla::LinuxYamlConfig& yamlConfig,
           const std::vector<std::string>& topics,
           const std::vector<std::string>& documentIds,
           unsigned int expirationTimeSec)
    : topics(topics), documentIds(documentIds),
      expirationTimeSec(expirationTimeSec) {
  if (this->documentIds.empty()) {
    this->documentIds = topics.size() == 1 ?
        std::vector<std::string>{"default"} : topics;
  }
  if (this->documentIds.size() != topics.size() ||
      std::set<std::string>(this->documentIds.begin(),
                            this->documentIds.end()).size() != topics.size()) {
    throw std::invalid_argument("Invalid or duplicate MQTT document ids");
  }
  client = Supla::LinuxMqttClient::getInstance(yamlConfig);
  for (auto& topic : topics) {
    SUPLA_LOG_DEBUG("Mark topic %s to subscribe", topic.c_str());
    client->subscribeTopic(topic);
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

std::vector<std::string> Mqtt::getDocumentIds() const {
  return documentIds;
}

std::vector<Document> Mqtt::getDocuments() {
  const auto messages = client->getMessages(topics);
  const uint32_t now = millis();
  std::vector<Document> documents;
  for (size_t i = 0; i < messages.size(); ++i) {
    const auto& message = messages[i];
    const bool fresh = !expirationTimeSec ||
        static_cast<uint32_t>(now - message.updatedMs) <=
            static_cast<uint64_t>(expirationTimeSec) * 1000;
    documents.push_back({documentIds[i], message.content,
                         message.received, fresh});
  }
  return documents;
}

std::string Supla::Source::Mqtt::getContent() {
  if (expirationTimeSec) {
    const auto documents = getDocuments();
    latestMessage.clear();
    for (size_t i = 0; i < documents.size(); ++i) {
      const auto& document = documents[i];
      if (!document.received || !document.fresh) {
        latestMessage.clear();
        return latestMessage;
      }
      // Keep every position, including received empty payloads. Omitting any
      // document would assign the following payload to a different Simple key.
      if (i != 0) {
        latestMessage += "\n";
      }
      latestMessage += document.content;
    }
    return latestMessage;
  }

  std::string combinedMessages;
  for (const auto& topic : topics) {
    std::string currentMessage;
    if (client->getMessage(topic, currentMessage)) {
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
