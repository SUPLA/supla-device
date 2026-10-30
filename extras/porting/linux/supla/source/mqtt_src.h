// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_

#include <linux_yaml_config.h>
#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "source.h"

namespace Supla {
class LinuxMqttClient;
}

namespace Supla::Source {

class Mqtt : public Source {
 public:
  static constexpr uint32_t MaximumExpirationTimeSec = INT32_MAX / 1000;

  Mqtt(const Supla::LinuxYamlConfig& yamlConfig,
       const std::vector<std::string>& topics,
       const std::vector<std::string>& documentIds = {},
       unsigned int expirationTimeSec = 0);
  ~Mqtt();

  std::string getContent() override;
  bool isConnected() override;
  std::vector<Document> getDocuments() override;
  std::vector<std::string> getDocumentIds() const override;


 protected:
  struct Freshness {
    uint32_t revision = 0;
    uint32_t timestamp = 0;  // Zero means missing or expired for this source.
  };
  std::vector<Freshness> freshness;

  std::shared_ptr<Supla::LinuxMqttClient> client;
  std::string latestMessage;
  std::vector<std::string> topics;
  std::vector<std::string> documentIds;
  unsigned int expirationTimeSec = 0;
};
}  // namespace Supla::Source

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_
