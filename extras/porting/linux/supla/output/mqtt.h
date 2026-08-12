// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_MQTT_H_
#define EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_MQTT_H_

#include <memory>
#include <vector>
#include <string>

#include "linux_mqtt_client.h"
#include "output.h"

namespace Supla {
namespace Output {
class Mqtt : public Output {
 public:
  explicit Mqtt(const Supla::LinuxYamlConfig &yamlConfig,
                const char *topic,
                int qos);
  virtual ~Mqtt();

 protected:
  std::string controlTopic;
  std::shared_ptr<Supla::LinuxMqttClient> client;
  int qos;

 private:
  bool putContent(int payload) override;
  bool putContent(const std::string &payload) override;
  bool putContent(const std::vector<int> &payload) override;
  bool putContent(bool payload) override;
};
}  // namespace Output
}  // namespace Supla
#endif  // EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_MQTT_H_
