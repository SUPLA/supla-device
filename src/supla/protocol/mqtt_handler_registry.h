// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PROTOCOL_MQTT_HANDLER_REGISTRY_H_
#define SRC_SUPLA_PROTOCOL_MQTT_HANDLER_REGISTRY_H_

#include <stddef.h>

#include <supla/protocol/mqtt_channel_handler.h>

#ifndef SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS
#define SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS 8
#endif

namespace Supla {
namespace Protocol {

class MqttHandlerRegistry {
 public:
  static MqttHandlerRegistry &instance();

  bool registerHandler(MqttChannelHandler *handler);
  MqttChannelHandler *findHandler(int channelType) const;

#if SUPLA_TEST
  void clearForTests();
  size_t sizeForTests() const;
#endif

 private:
  MqttChannelHandler *handlers[SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS] = {};
  size_t handlerCount = 0;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_MQTT_HANDLER_REGISTRY_H_
