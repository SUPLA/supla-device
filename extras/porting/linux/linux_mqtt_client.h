//
// Created by Mariusz on 01.05.2024.
//

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
  static void publishCallback(void** unused, struct mqtt_response_publish* published);

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
