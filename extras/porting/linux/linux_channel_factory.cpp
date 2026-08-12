// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "linux_channel_factory.h"

#include <supla/log_wrapper.h>
#include <string>

Supla::Linux::ChannelFactoryRegistry&
Supla::Linux::ChannelFactoryRegistry::instance() {
  static ChannelFactoryRegistry registry;
  return registry;
}

bool Supla::Linux::ChannelFactoryRegistry::registerFactory(
    const std::string& pluginName,
    const std::string& typeName,
    ChannelFactory factory) {
  if (pluginName.empty() || typeName.empty() || !factory) {
    SUPLA_LOG_ERROR("Config: invalid Linux channel factory registration");
    return false;
  }

  if (factoriesByType.count(typeName) > 0) {
    SUPLA_LOG_ERROR("Config: duplicate Linux channel type \"%s\"",
                    typeName.c_str());
    return false;
  }

  factoriesByType[typeName] = ChannelFactoryEntry{
      pluginName,
      typeName,
      factory,
  };
  return true;
}

const Supla::Linux::ChannelFactoryEntry*
Supla::Linux::ChannelFactoryRegistry::findByType(
    const std::string& typeName) const {
  auto it = factoriesByType.find(typeName);
  if (it == factoriesByType.end()) {
    return nullptr;
  }
  return &it->second;
}

bool Supla::Linux::ChannelFactoryRegistry::hasPlugin(
    const std::string& pluginName) const {
  for (const auto& entry : factoriesByType) {
    if (entry.second.pluginName == pluginName) {
      return true;
    }
  }
  return false;
}

void Supla::Linux::ChannelFactoryRegistry::clear() {
  factoriesByType.clear();
}

Supla::Linux::ChannelFactoryRegistrar::ChannelFactoryRegistrar(
    const std::string& pluginName,
    const std::string& typeName,
    ChannelFactory factory) {
  ChannelFactoryRegistry::instance().registerFactory(pluginName,
                                                     typeName,
                                                     factory);
}
