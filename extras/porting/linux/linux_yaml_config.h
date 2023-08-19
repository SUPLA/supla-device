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

/*
 * Linux YAML based config file.
 *
 * It contain two parts:
 * 1. etc/supla-device.yaml file with user defined read-only configuration
 * 2. var/supla-device.yaml file used for supla-device read/write storage for
 *    GUID and AUTHKEY
 *
 * All "set" methods are executed against read/write storage or disabled.
 * It overrides some "get" methods to use read only storage with different
 * keys than those used by default in supla-device.
 *
 * Example config yaml file. Names and values are case-sensitive.
 * Please pay attantion to spaces, as those are important in yaml files.

name: Device name
# log_level - optional, values: info (default), debug, verbose
log_level: debug

supla:
  server: svrXYZ.supla.org
  mail: mail@user.com

# Order in channels is important. First item is channel 0, then 1, etc.
# Supla Cloud doesn't like changes in channel list, so you can only add channels
# at the end of the list. Otherwise, you will have to remove device from
# Cloud and register it again (whole measurement history will be lost).
channels:
  - type: VirtualRelay

  - type: ImpulseCounterParsed
    source:
      type: File
      file: /home/something
    parser:
      type: Json
      counter: total_m3

 */

#ifndef EXTRAS_PORTING_LINUX_LINUX_YAML_CONFIG_H_
#define EXTRAS_PORTING_LINUX_LINUX_YAML_CONFIG_H_

#include <supla/channel_element.h>
#include <supla/parser/parser.h>
#include <supla/sensor/electricity_meter_parsed.h>
#include <supla/sensor/sensor_parsed.h>
#include <supla/source/source.h>
#include <supla/storage/config.h>
#include <yaml-cpp/yaml.h>
#include <supla/storage/key_value.h>

#include <map>
#include <string>

namespace Supla {

class LinuxYamlConfig : public KeyValue {
 public:
  explicit LinuxYamlConfig(const std::string& file);
  virtual ~LinuxYamlConfig();

  bool isDebug();
  bool isVerbose();

  bool loadChannels();

  bool init() override;

  bool generateGuidAndAuthkey() override;
  bool isConfigModeSupported() override;

  // Generic getters and setters
  // getUInt8 may be read from yaml or from KeyValue storage, so we override
  // this method. It may be extended to other parameters in future (if needed).
  bool getUInt8(const char* key, uint8_t* result) override;

  void commit() override;

  // Device generic config
  bool setGUID(const char* guid) override;
  bool getGUID(char* result) override;
  bool setAuthKey(const char* authkey) override;
  bool getAuthKey(char* result) override;

  bool setDeviceName(const char* name) override;  // disabled
  bool getDeviceName(char* result) override;

  // Supla protocol config
  bool setSuplaCommProtocolEnabled(bool enabled) override;  // disabled
  bool setSuplaServer(const char* server) override;         // disabled
  bool setSuplaServerPort(int32_t port) override;           // disabled
  bool setEmail(const char* email) override;                // disabled

  bool isSuplaCommProtocolEnabled() override;
  bool getSuplaServer(char* result) override;
  int32_t getSuplaServerPort() override;
  bool getEmail(char* result) override;

  int getProtoVersion();

  std::string getStateFilesPath();

 protected:
  bool parseChannel(const YAML::Node& ch, int channelNumber);
  Supla::Parser::Parser* addParser(const YAML::Node& parser,
                                   Supla::Source::Source* src);
  Supla::Source::Source* addSource(const YAML::Node& ch);

  bool addVirtualRelay(const YAML::Node& ch, int channelNumber);
  bool addCmdRelay(const YAML::Node& ch, int channelNumber,
      Supla::Parser::Parser*);
  bool addFronius(const YAML::Node& ch, int channelNumber);
  bool addAfore(const YAML::Node& ch, int channelNumber);
  bool addHvac(const YAML::Node& ch, int channelNumber);
  bool addThermometerParsed(const YAML::Node& ch,
                            int channelNumber,
                            Supla::Parser::Parser* parser);
  bool addImpulseCounterParsed(const YAML::Node& ch,
                               int channelNumber,
                               Supla::Parser::Parser* parser);
  bool addElectricityMeterParsed(const YAML::Node& ch,
                                 int channelNumber,
                                 Supla::Parser::Parser* parser);
  bool addBinaryParsed(const YAML::Node& ch,
                       int channelNumber,
                       Supla::Parser::Parser* parser);
  bool addThermHygroMeterParsed(const YAML::Node& ch,
                            int channelNumber,
                            Supla::Parser::Parser* parser);
  bool addHumidityParsed(const YAML::Node& ch,
                            int channelNumber,
                            Supla::Parser::Parser* parser);
  bool addPressureParsed(const YAML::Node& ch,
                            int channelNumber,
                            Supla::Parser::Parser* parser);
  bool addRainParsed(const YAML::Node& ch,
                            int channelNumber,
                            Supla::Parser::Parser* parser);
  bool addWindParsed(const YAML::Node& ch,
                            int channelNumber,
                            Supla::Parser::Parser* parser);
  void loadGuidAuthFromPath(const std::string& path);
  bool saveGuidAuth(const std::string& path);
  bool addStateParser(const YAML::Node& ch,
                      Supla::Sensor::SensorParsedBase* sensor,
                      Supla::Parser::Parser* parser,
                      bool mandatory);
  bool addActionTriggerActions(const YAML::Node& ch,
                      Supla::Sensor::SensorParsedBase *sensor,
                      bool mandatory);
  bool addActionTriggerParsed(const YAML::Node& ch,
                      int channnelNumber);

  std::string file;
  YAML::Node config;
  std::string stateFilesLocaltion;

  std::string guid;
  std::string authkey;
  std::map<std::string, int> channelNames;
  std::map<std::string, int> parserNames;
  std::map<std::string, int> sourceNames;
  std::map<int, Supla::Parser::Parser*> parsers;
  std::map<int, Supla::Source::Source*> sources;

  int paramCount = 0;
  int parserCount = 0;
  int sourceCount = 0;

  bool initDone = false;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_YAML_CONFIG_H_
