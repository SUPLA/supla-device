// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PROTOCOL_MQTT_TOPIC_H_
#define SRC_SUPLA_PROTOCOL_MQTT_TOPIC_H_

namespace Supla {
namespace Protocol {

#define MAX_TOPIC_LEN 256

class MqttTopic {
 public:
  explicit MqttTopic(const char *);
  MqttTopic();

  MqttTopic operator/(const char *);
  MqttTopic& operator/=(const char *);

  MqttTopic& operator+=(const char *);

  MqttTopic operator/(const int);
  MqttTopic& operator/=(const int);

  void append(const char *);

  const char *c_str();

 protected:
  char data[MAX_TOPIC_LEN] = {};
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_MQTT_TOPIC_H_
