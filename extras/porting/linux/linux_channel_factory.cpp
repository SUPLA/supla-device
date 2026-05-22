/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

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
