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

#ifndef SUPLA_DEVICE_LINUX_MQTT_H
#define SUPLA_DEVICE_LINUX_MQTT_H

#include <linux_yaml_config.h>

#include <string>

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
