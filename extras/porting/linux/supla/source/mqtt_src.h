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

 protected:
  std::shared_ptr<Supla::LinuxMqttClient> client;
  std::string latestMessage;
  std::vector<std::string> topics;
  int qos;
};
}  // namespace Supla::Source

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_MQTT_SRC_H_
