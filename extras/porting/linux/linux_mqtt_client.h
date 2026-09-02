// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_MQTT_CLIENT_H_
#define EXTRAS_PORTING_LINUX_LINUX_MQTT_CLIENT_H_

#include <linux_yaml_config.h>
#include <mqtt.h>
#include <mqtt_pal.h>
#include <yaml-cpp/yaml.h>

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

  bool isConnected() const;

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
