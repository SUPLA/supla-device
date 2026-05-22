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

#ifndef EXTRAS_PORTING_LINUX_LINUX_CHANNEL_FACTORY_H_
#define EXTRAS_PORTING_LINUX_LINUX_CHANNEL_FACTORY_H_

#include <supla/output/output.h>
#include <supla/parser/parser.h>
#include <supla/payload/payload.h>
#include <supla/source/source.h>
#include <yaml-cpp/yaml.h>

#include <functional>
#include <map>
#include <string>

namespace Supla {
class LinuxYamlConfig;

namespace Linux {

struct ChannelFactoryContext {
  LinuxYamlConfig& config;
  const YAML::Node& channel;
  int channelNumber = -1;
  Supla::Source::Source* source = nullptr;
  Supla::Parser::Parser* parser = nullptr;
  Supla::Output::Output* output = nullptr;
  Supla::Payload::Payload* payload = nullptr;
};

using ChannelFactory = std::function<bool(const ChannelFactoryContext&)>;

struct ChannelFactoryEntry {
  std::string pluginName;
  std::string typeName;
  ChannelFactory factory;
};

class ChannelFactoryRegistry {
 public:
  static ChannelFactoryRegistry& instance();

  bool registerFactory(const std::string& pluginName,
                       const std::string& typeName,
                       ChannelFactory factory);

  const ChannelFactoryEntry* findByType(const std::string& typeName) const;
  bool hasPlugin(const std::string& pluginName) const;
  void clear();

 private:
  std::map<std::string, ChannelFactoryEntry> factoriesByType;
};

class ChannelFactoryRegistrar {
 public:
  ChannelFactoryRegistrar(const std::string& pluginName,
                          const std::string& typeName,
                          ChannelFactory factory);
};

}  // namespace Linux
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_CHANNEL_FACTORY_H_
