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

#include "mqtt_src.h"

#include <linux_mqtt_client.h>
#include <linux_yaml_config.h>
#include <supla/log_wrapper.h>

namespace Supla::Source {

Mqtt::Mqtt(const Supla::LinuxYamlConfig& yamlConfig,
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
    SUPLA_LOG_DEBUG(
        "get latest message %s for %s", latestMessage.c_str(), topic.c_str());
  }
  return latestMessage;
}

}  // namespace Supla::Source
