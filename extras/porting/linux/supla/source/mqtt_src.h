//
// Created by Mariusz on 01.05.2024.
//

#ifndef SUPLA_DEVICE_LINUX_MQTT_H
#define SUPLA_DEVICE_LINUX_MQTT_H

#include <string>
#include <linux_yaml_config.h>
#include "linux_mqtt_client.h"

#include "source.h"

namespace Supla::Source {

class Mqtt : public Source {
 public:
  Mqtt(Supla::LinuxYamlConfig& yamlConfig, const std::string& topic, int qos);
  ~Mqtt();

  std::string getContent() override;

 protected:
  std::shared_ptr<Supla::LinuxMqttClient> client;
  std::string latestMessage;
  std::string topic;
  int qos;
};
}  // namespace Supla::Source

#endif  // SUPLA_DEVICE_LINUX_MQTT_H
