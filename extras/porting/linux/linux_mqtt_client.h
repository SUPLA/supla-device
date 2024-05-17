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

#ifndef EXTRAS_PORTING_LINUX_LINUX_MQTT_CLIENT_H_
#define EXTRAS_PORTING_LINUX_LINUX_MQTT_CLIENT_H_

#include <linux_yaml_config.h>
#include <mqtt.h>
#include <mqtt_pal.h>
#include <yaml-cpp/yaml.h>

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace Supla {

class LinuxMqttClient {
 public:
  ~LinuxMqttClient();

  static std::shared_ptr<LinuxMqttClient>& getInstance(
      const LinuxYamlConfig& yamlConfig);
  static std::shared_ptr<LinuxMqttClient>& getInstance();

  static void start();

  void unsubscribeTopic(const std::string& topic);

  void subscribeTopic(const std::string& topic, int qos);

  static void publishCallback(void**, struct mqtt_response_publish* published);

  enum MQTTErrors publish(const std::string& topic,
                          const std::string& message,
                          int qos);

  struct mqtt_client* mq_client = nullptr;

  static std::unordered_map<std::string, std::string> topics;

  bool useSSL = false;
  bool verifyCA = false;
  std::string fileCA;

 private:
  explicit LinuxMqttClient(const LinuxYamlConfig& yamlConfig);

  LinuxMqttClient(const LinuxMqttClient&) = delete;
  LinuxMqttClient& operator=(const LinuxMqttClient&) = delete;

  int mqttClientInit();

  std::string host;
  int port;
  std::string username;
  std::string password;
  std::string clientName;

  std::array<uint8_t, 8192> sendbuf;
  std::array<uint8_t, 2048> recvbuf;

  static std::shared_ptr<Supla::LinuxMqttClient> instance;
};

}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_MQTT_CLIENT_H_
