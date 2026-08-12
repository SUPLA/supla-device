// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PROTOCOL_MQTT_CHANNEL_HANDLER_H_
#define SRC_SUPLA_PROTOCOL_MQTT_CHANNEL_HANDLER_H_

#include <stdint.h>

namespace Supla {
class Element;
namespace Protocol {

class Mqtt;

class MqttChannelHandler {
 public:
  virtual ~MqttChannelHandler() = default;

  virtual int mqttHandledChannelType() const = 0;

  virtual void mqttPublishChannelState(Mqtt *mqtt, Supla::Element *element) = 0;
  virtual void mqttSubscribeChannel(Mqtt *mqtt, Supla::Element *element) = 0;
  virtual bool mqttProcessData(Mqtt *mqtt,
                               const char *topic_part,
                               const char *payload,
                               Supla::Element *element) = 0;
  virtual void mqttPublishHADiscovery(Mqtt *mqtt, Supla::Element *element) = 0;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_MQTT_CHANNEL_HANDLER_H_
