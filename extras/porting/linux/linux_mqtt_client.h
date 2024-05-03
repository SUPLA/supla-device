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

#ifndef SUPLA_DEVICE_LINUX_LINUX_MQTT_CLIENT_H
#define SUPLA_DEVICE_LINUX_LINUX_MQTT_CLIENT_H

#include <mqtt-c/mqtt.h>
#include <mqtt-c/mqtt_pal.h>

#include <functional>
#include <string>

#include "linux_yaml_config.h"
#include "yaml-cpp/yaml.h"

namespace Supla {

class LinuxMqttClient {
 public:
  ~LinuxMqttClient();

  static std::shared_ptr<LinuxMqttClient>& getInstance(
      LinuxYamlConfig& yamlConfig);
  static std::shared_ptr<LinuxMqttClient>& getInstance();

  static void start();

  void unsubscribeTopic(const std::string& topic);

  void subscribeTopic(const std::string& topic, int qos);

  void setMessageHandler(std::function<void(const std::string&)> handler);
  static void publishCallback(void** unused,
                              struct mqtt_response_publish* published);

  struct mqtt_client* mq_client;

  static std::unordered_map<std::string, std::string> topics;

  bool useSSL;
  bool verifyCA;
  std::string fileCA;

 private:
  explicit LinuxMqttClient(LinuxYamlConfig& yamlConfig);

  LinuxMqttClient(const LinuxMqttClient&) = delete;
  LinuxMqttClient& operator=(const LinuxMqttClient&) = delete;

  int mqttClientInit();

  std::string host;
  int port;
  std::string username;
  std::string password;
  std::string clientName;

  uint8_t* sendbuf;
  size_t sendbufsz;
  uint8_t* recvbuf;
  size_t recvbufsz;

  static std::shared_ptr<Supla::LinuxMqttClient> instance;
};

}  // namespace Supla

#endif  // SUPLA_DEVICE_LINUX_LINUX_MQTT_CLIENT_H
