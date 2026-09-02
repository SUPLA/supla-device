// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/protocol/mqtt_handler_registry.h>

namespace Supla {
namespace Protocol {

MqttHandlerRegistry &MqttHandlerRegistry::instance() {
  static MqttHandlerRegistry registry;
  return registry;
}

bool MqttHandlerRegistry::registerHandler(MqttChannelHandler *handler) {
  if (handler == nullptr) {
    return false;
  }

  for (size_t i = 0; i < handlerCount; i++) {
    if (handlers[i] == handler || handlers[i]->mqttHandledChannelType() ==
                                      handler->mqttHandledChannelType()) {
      return true;
    }
  }

  if (handlerCount >= SUPLA_MQTT_HANDLER_REGISTRY_MAX_HANDLERS) {
    return false;
  }

  handlers[handlerCount++] = handler;
  return true;
}

MqttChannelHandler *MqttHandlerRegistry::findHandler(int channelType) const {
  for (size_t i = 0; i < handlerCount; i++) {
    if (handlers[i]->mqttHandledChannelType() == channelType) {
      return handlers[i];
    }
  }
  return nullptr;
}

#if SUPLA_TEST
void MqttHandlerRegistry::clearForTests() {
  handlerCount = 0;
}

size_t MqttHandlerRegistry::sizeForTests() const {
  return handlerCount;
}
#endif

}  // namespace Protocol
}  // namespace Supla
