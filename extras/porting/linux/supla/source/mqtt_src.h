// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_

#include <linux_yaml_config.h>

#include <memory>
#include <string>
#include <vector>

#include "linux_mqtt_client.h"
#include "source.h"

namespace Supla::Source {

class Mqtt : public Source {
 public:
  Mqtt(const Supla::LinuxYamlConfig& yamlConfig,
       const std::vector<std::string>& topics,
       int qos);
  ~Mqtt();

  std::string getContent() override;
  bool isConnected() override;

 protected:
  std::shared_ptr<Supla::LinuxMqttClient> client;
  std::string latestMessage;
  std::vector<std::string> topics;
  int qos;
};
}  // namespace Supla::Source

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_
