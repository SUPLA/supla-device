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

#include "linux_yaml_config.h"

#include <supla-common/proto.h>
#include <supla/control/action_trigger_parsed.h>
#include <supla/control/cmd_relay.h>
#include <supla/control/virtual_relay.h>
#include <supla/log_wrapper.h>
#include <supla/network/ip_address.h>
#include <supla/parser/json.h>
#include <supla/parser/parser.h>
#include <supla/parser/simple.h>
#include <supla/pv/afore.h>
#include <supla/pv/fronius.h>
#include <supla/sensor/binary_parsed.h>
#include <supla/sensor/electricity_meter_parsed.h>
#include <supla/sensor/humidity_parsed.h>
#include <supla/sensor/impulse_counter_parsed.h>
#include <supla/sensor/pressure_parsed.h>
#include <supla/sensor/rain_parsed.h>
#include <supla/sensor/thermometer_parsed.h>
#include <supla/sensor/wind_parsed.h>
#include <supla/sensor/general_purpose_measurement_parsed.h>
#include <supla/sensor/general_purpose_meter_parsed.h>
#include <supla/source/cmd.h>
#include <supla/source/file.h>
#include <supla/source/source.h>
#include <supla/tools.h>

#include <chrono>  // NOLINT(build/c++11)
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include "supla/control/action_trigger.h"
#include "supla/control/hvac_parsed.h"
#include "supla/sensor/sensor_parsed.h"
#include "supla/sensor/therm_hygro_meter_parsed.h"
#include "supla/storage/key_value.h"

namespace Supla {
const char Multiplier[] = "multiplier";
const char MultiplierTemp[] = "multiplier_temp";
const char MultiplierHumi[] = "multiplier_humi";

const char GuidAuthFileName[] = "/guid_auth.yaml";
const char ReadWriteConfigStorage[] = "/config_storage.bin";
const char GuidKey[] = "guid";
const char AuthKeyKey[] = "authkey";
};  // namespace Supla

#define SUPLA_LINUX_CONFIG_BUF_SIZE (1024 * 1024)

Supla::LinuxYamlConfig::LinuxYamlConfig(const std::string& file) : file(file) {
}

Supla::LinuxYamlConfig::~LinuxYamlConfig() {
}

bool Supla::LinuxYamlConfig::init() {
  if (initDone) {
    return true;
  }

  initDone = true;
  if (config.size() == 0) {
    try {
      config = YAML::LoadFile(file);
      loadGuidAuthFromPath(getStateFilesPath());
    } catch (const YAML::Exception& ex) {
      SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
      return false;
    }
  }
  // check if KeyValue was initialized earlier
  if (first) {
    SUPLA_LOG_WARNING("init called on non empty database. Aborting");
    // init can be done only on empty storage
    return false;
  }

  std::string readWriteConfigFilePath =
      getStateFilesPath() + Supla::ReadWriteConfigStorage;

  uint8_t buf[SUPLA_LINUX_CONFIG_BUF_SIZE] = {};
  if (std::filesystem::exists(readWriteConfigFilePath)) {
    SUPLA_LOG_INFO("Read write config storage file: %s",
                   readWriteConfigFilePath.c_str());

    std::ifstream rwConfigFile(readWriteConfigFilePath,
                            std::ifstream::in | std::ios::binary);

    unsigned char c = rwConfigFile.get();

    int bytesRead = 0;
    for (; rwConfigFile.good() && bytesRead < SUPLA_LINUX_CONFIG_BUF_SIZE;
         bytesRead++) {
      buf[bytesRead] = c;
      c = rwConfigFile.get();
    }

    rwConfigFile.close();

    if (bytesRead >= SUPLA_LINUX_CONFIG_BUF_SIZE) {
      SUPLA_LOG_ERROR("Read write config: buffer overflow - file is too big");
      return false;
    }

    SUPLA_LOG_INFO("Read write config: initializing storage from file...");
    return initFromMemory(buf, bytesRead);
  } else {
    SUPLA_LOG_INFO(
        "Read write config: config file missing, starting with empty database");
  }

  return true;
}

bool Supla::LinuxYamlConfig::isDebug() {
  try {
    if (config["log_level"]) {
      auto logLevel = config["log_level"].as<std::string>();
      if (logLevel == "debug") {
        return true;
      }
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

bool Supla::LinuxYamlConfig::isVerbose() {
  try {
    if (config["log_level"]) {
      auto logLevel = config["log_level"].as<std::string>();
      if (logLevel == "verbose") {
        return true;
      }
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

bool Supla::LinuxYamlConfig::generateGuidAndAuthkey() {
  char guid[SUPLA_GUID_SIZE] = {};
  char authkey[SUPLA_AUTHKEY_SIZE] = {};

  unsigned int randSeed = static_cast<unsigned int>(
      std::chrono::system_clock::now().time_since_epoch().count());

  std::mt19937 randGen(randSeed);
  std::uniform_int_distribution<unsigned char> distribution(0, 255);

  for (int i = 0; i < SUPLA_GUID_SIZE; i++) {
    guid[i] = static_cast<char>(distribution(randGen));
  }

  for (int i = 0; i < SUPLA_AUTHKEY_SIZE; i++) {
    authkey[i] = distribution(randGen);
  }

  if (isArrayEmpty(guid, SUPLA_GUID_SIZE)) {
    SUPLA_LOG_ERROR("Failed to generate GUID");
    return false;
  }
  if (isArrayEmpty(authkey, SUPLA_AUTHKEY_SIZE)) {
    SUPLA_LOG_ERROR("Failed to generate AUTHKEY");
    return false;
  }

  setGUID(guid);
  setAuthKey(authkey);

  return saveGuidAuth(getStateFilesPath());
}

// Currently "security_level" is read from yaml config and for all other
// parameters we fallback to KeyValue getUInt8.
// This method will first look for param in yaml, and if it is missing, it
// will look for it in KeyValue storage
bool Supla::LinuxYamlConfig::getUInt8(const char* key, uint8_t* result) {
  try {
    if (config[key]) {
      *result = static_cast<uint8_t>(config[key].as<unsigned int>());
      return true;
    } else {
      return Supla::KeyValue::getUInt8(key, result);
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

void Supla::LinuxYamlConfig::commit() {
  uint8_t buf[SUPLA_LINUX_CONFIG_BUF_SIZE] = {};

  size_t dataSize = serializeToMemory(buf, SUPLA_LINUX_CONFIG_BUF_SIZE);

  std::ofstream rwConfigFile(
      getStateFilesPath() + Supla::ReadWriteConfigStorage,
      std::ofstream::out | std::ios::binary);

  for (int i = 0; i < dataSize; i++) {
    rwConfigFile << buf[i];
  }

  rwConfigFile.close();
}

bool Supla::LinuxYamlConfig::setDeviceName(const char* name) {
  SUPLA_LOG_WARNING("setDeviceName is not supported on this platform");
  return false;
}

bool Supla::LinuxYamlConfig::setSuplaCommProtocolEnabled(bool enabled) {
  SUPLA_LOG_WARNING(
      "setSuplaCommProtocolEnabled is not supported on this platform");
  return false;
}
bool Supla::LinuxYamlConfig::setSuplaServer(const char* server) {
  SUPLA_LOG_WARNING("setSuplaServer is not supported on this platform");
  return false;
}
bool Supla::LinuxYamlConfig::setSuplaServerPort(int32_t port) {
  SUPLA_LOG_WARNING("setSuplaServerPort is not supported on this platform");
  return false;
}
bool Supla::LinuxYamlConfig::setEmail(const char* email) {
  SUPLA_LOG_WARNING("setEmail is not supported on this platform");
  return false;
}

bool Supla::LinuxYamlConfig::getDeviceName(char* result) {
  try {
    if (config["name"]) {
      auto deviceName = config["name"].as<std::string>();
      strncpy(result, deviceName.c_str(), SUPLA_DEVICE_NAME_MAXSIZE);
      return true;
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

bool Supla::LinuxYamlConfig::isSuplaCommProtocolEnabled() {
  return true;
}

bool Supla::LinuxYamlConfig::getSuplaServer(char* result) {
  try {
    if (config["supla"] && config["supla"]["server"]) {
      auto server = config["supla"]["server"].as<std::string>();
      strncpy(result, server.c_str(), SUPLA_SERVER_NAME_MAXSIZE);
      return true;
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

int32_t Supla::LinuxYamlConfig::getSuplaServerPort() {
  try {
    if (config["port"]) {
      auto port = config["port"].as<int>();
      return port;
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return 2016;
}

bool Supla::LinuxYamlConfig::getEmail(char* result) {
  try {
    if (config["supla"] && config["supla"]["mail"]) {
      auto mail = config["supla"]["mail"].as<std::string>();
      strncpy(result, mail.c_str(), SUPLA_EMAIL_MAXSIZE);
      return true;
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

int Supla::LinuxYamlConfig::getProtoVersion() {
  try {
    if (config["supla"] && config["supla"]["proto"]) {
      auto version = config["supla"]["proto"].as<int>();
      return version;
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return 20;
}

bool Supla::LinuxYamlConfig::setGUID(const char* guidRaw) {
  char guidHex[SUPLA_GUID_HEXSIZE] = {};
  generateHexString(guidRaw, guidHex, SUPLA_GUID_SIZE);
  guid = std::string(guidHex);
  return true;
}

bool Supla::LinuxYamlConfig::getGUID(char* result) {
  if (guid.length()) {
    hexStringToArray(guid.c_str(), result, SUPLA_GUID_SIZE);
    return true;
  }
  return false;
}

bool Supla::LinuxYamlConfig::setAuthKey(const char* authkeyRaw) {
  char authkeyHex[SUPLA_AUTHKEY_HEXSIZE] = {};
  generateHexString(authkeyRaw, authkeyHex, SUPLA_AUTHKEY_SIZE);
  authkey = std::string(authkeyHex);
  return true;
}

bool Supla::LinuxYamlConfig::getAuthKey(char* result) {
  if (authkey.length()) {
    hexStringToArray(authkey.c_str(), result, SUPLA_AUTHKEY_SIZE);
    return true;
  }
  return false;
}

bool Supla::LinuxYamlConfig::loadChannels() {
  try {
    if (config["channels"]) {
      auto channels = config["channels"];
      int channelCount = 0;
      for (auto it : channels) {
        if (!parseChannel(it, channelCount)) {
          SUPLA_LOG_ERROR("Config: parsing channel %d failed", channelCount);
          return false;
        }
        channelCount++;
      }
      if (channelCount == 0) {
        SUPLA_LOG_ERROR("Config: \"channels\" section missing in file");
        return false;
      }
      return true;
    } else {
      SUPLA_LOG_ERROR("Config: \"channels\" section missing in file");
      return false;
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  return false;
}

bool Supla::LinuxYamlConfig::parseChannel(const YAML::Node& ch,
                                          int channelNumber) {
  paramCount = 0;
  if (ch["type"]) {
    paramCount++;
    std::string type = ch["type"].as<std::string>();

    Supla::Source::Source* source = nullptr;
    Supla::Parser::Parser* parser = nullptr;

    if (ch["source"]) {
      paramCount++;
      if (!(source = addSource(ch["source"]))) {
        SUPLA_LOG_ERROR("Adding source failed");
        return false;
      }
    }

    if (ch["parser"]) {
      paramCount++;
      if (!(parser = addParser(ch["parser"], source))) {
        SUPLA_LOG_ERROR("Adding parser failed");
        return false;
      }
      parserCount++;
    }

    if (ch["name"]) {  // optional
      paramCount++;
      std::string name = ch["name"].as<std::string>();
      channelNames[name] = channelNumber;
    }

    if (type == "VirtualRelay") {
      return addVirtualRelay(ch, channelNumber);
    } else if (type == "CmdRelay") {
      return addCmdRelay(ch, channelNumber, parser);
    } else if (type == "Fronius") {
      return addFronius(ch, channelNumber);
    } else if (type == "Afore") {
      return addAfore(ch, channelNumber);
    } else if (type == "Hvac") {
      return addHvac(ch, channelNumber);
    } else if (type == "ThermometerParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addThermometerParsed(ch, channelNumber, parser);
    } else if (type == "GeneralPurposeMeasurementParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addGeneralPurposeMeasurementParsed(ch, channelNumber, parser);
    } else if (type == "GeneralPurposeMeterParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addGeneralPurposeMeterParsed(ch, channelNumber, parser);
    } else if (type == "ImpulseCounterParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addImpulseCounterParsed(ch, channelNumber, parser);
    } else if (type == "ElectricityMeterParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addElectricityMeterParsed(ch, channelNumber, parser);
    } else if (type == "BinaryParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addBinaryParsed(ch, channelNumber, parser);
    } else if (type == "HumidityParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addHumidityParsed(ch, channelNumber, parser);
    } else if (type == "PressureParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addPressureParsed(ch, channelNumber, parser);
    }
    if (type == "WindParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addWindParsed(ch, channelNumber, parser);
    }
    if (type == "RainParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addRainParsed(ch, channelNumber, parser);
    } else if (type == "ThermHygroMeterParsed") {
      if (!parser) {
        SUPLA_LOG_ERROR("Channel[%d] config: missing parser", channelNumber);
        return false;
      }
      return addThermHygroMeterParsed(ch, channelNumber, parser);
    } else if (type == "ActionTriggerParsed") {
      return addActionTriggerParsed(ch, channelNumber);
    } else {
      SUPLA_LOG_ERROR("Channel[%d] config: unknown type \"%s\"",
                      channelNumber,
                      type.c_str());
      return false;
    }

    if (ch.size() > paramCount) {
      SUPLA_LOG_WARNING("Channel[%d] config: too many parameters",
                        channelNumber);
    }
    return true;

  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing mandatory \"type\" parameter",
                    channelNumber);
  }
  return false;
}

bool Supla::LinuxYamlConfig::addVirtualRelay(const YAML::Node& ch,
                                             int channelNumber) {
  SUPLA_LOG_INFO("Channel[%d] config: adding VirtualRelay", channelNumber);
  auto vr = new Supla::Control::VirtualRelay();
  if (ch["initial_state"]) {
    paramCount++;
    auto initialState = ch["initial_state"].as<std::string>();
    if (initialState == "on") {
      vr->setDefaultStateOn();
    } else if (initialState == "off") {
      vr->setDefaultStateOff();
    } else if (initialState == "restore") {
      vr->setDefaultStateRestore();
    }
  }
  return true;
}

bool Supla::LinuxYamlConfig::addCmdRelay(const YAML::Node& ch,
                                         int channelNumber,
                                         Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding CmdRelay", channelNumber);
  auto cr = new Supla::Control::CmdRelay(parser);
  if (ch["initial_state"]) {
    paramCount++;
    auto initialState = ch["initial_state"].as<std::string>();
    if (initialState == "on") {
      cr->setDefaultStateOn();
    } else if (initialState == "off") {
      cr->setDefaultStateOff();
    } else if (initialState == "restore") {
      cr->setDefaultStateRestore();
    }
  }

  if (ch["offline_on_invalid_state"]) {
    paramCount++;
    auto useOfflineOnInvalidState = ch["offline_on_invalid_state"].as<bool>();
    cr->setUseOfflineOnInvalidState(useOfflineOnInvalidState);
  }

  if (ch["cmd_on"]) {
    paramCount++;
    auto cmdOn = ch["cmd_on"].as<std::string>();
    cr->setCmdOn(cmdOn);
  }
  if (ch["cmd_off"]) {
    paramCount++;
    auto cmdOff = ch["cmd_off"].as<std::string>();
    cr->setCmdOff(cmdOff);
  }

  if (!addStateParser(ch, cr, parser, false)) {
    return false;
  }

  if (!addActionTriggerActions(ch, cr, false)) {
    return false;
  }

  return true;
}

bool Supla::LinuxYamlConfig::addFronius(const YAML::Node& ch,
                                        int channelNumber) {
  int port = 80;
  int deviceId = 1;
  if (ch["port"]) {
    paramCount++;
    port = ch["port"].as<int>();
  }
  if (ch["device_id"]) {
    paramCount++;
    deviceId = ch["device_id"].as<int>();
  }

  if (ch["ip"]) {  // mandatory
    paramCount++;
    std::string ip = ch["ip"].as<std::string>();
    SUPLA_LOG_INFO(
        "Channel[%d] config: adding Fronius with IP %s, port: %d, deviceId: %d",
        channelNumber,
        ip.c_str(),
        port,
        deviceId);

    IPAddress ipAddr(ip);
    new Supla::PV::Fronius(ipAddr, port, deviceId);
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing mandatory \"ip\" parameter",
                    channelNumber);
    return false;
  }

  return true;
}

bool Supla::LinuxYamlConfig::addAfore(const YAML::Node& ch, int channelNumber) {
  int port = 80;
  if (ch["port"]) {
    paramCount++;
    port = ch["port"].as<int>();
  }

  std::string loginAndPassword;

  if (ch["login_and_password"]) {
    paramCount++;
    loginAndPassword = ch["login_and_password"].as<std::string>();
  } else {
    SUPLA_LOG_ERROR(
        "Channel[%d] config: missing mandatory"
        " \"login_and_password\" parameter",
        channelNumber);
    return false;
  }

  if (ch["ip"]) {  // mandatory
    paramCount++;
    std::string ip = ch["ip"].as<std::string>();
    SUPLA_LOG_INFO(
        "Channel[%d] config: adding Afore with IP %s, port: %d,"
        " login_and_password: %s",
        channelNumber,
        ip.c_str(),
        port,
        loginAndPassword.c_str());

    IPAddress ipAddr(ip);
    new Supla::PV::Afore(ipAddr, port, loginAndPassword.c_str());
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing mandatory \"ip\" parameter",
                    channelNumber);
    return false;
  }

  return true;
}

bool Supla::LinuxYamlConfig::addHvac(const YAML::Node& ch, int channelNumber) {
  SUPLA_LOG_INFO("Channel[%d] config: adding Hvac", channelNumber);
  int mainThermometerChannelNo = -1;
  int auxThermometerChannelNo = -1;
  int binarySensorChannelNo = -1;
  std::string cmdOn;
  std::string cmdOff;
  std::string cmdOnSecondary;
  std::string cmdOffSecondary;
  if (ch["cmd_on"]) {
    paramCount++;
    cmdOn = ch["cmd_on"].as<std::string>();
  } else {
    SUPLA_LOG_ERROR(
        "Channel[%d] config: missing mandatory \"cmd_on\" parameter",
        channelNumber);
    return false;
  }
  if (ch["cmd_off"]) {
    paramCount++;
    cmdOff = ch["cmd_off"].as<std::string>();
  } else {
    SUPLA_LOG_ERROR(
        "Channel[%d] config: missing mandatory \"cmd_off\" parameter",
        channelNumber);
    return false;
  }
  if (ch["cmd_on_secondary"]) {
    paramCount++;
    cmdOnSecondary = ch["cmd_on_secondary"].as<std::string>();
  }
  if (ch["cmd_off_secondary"]) {
    paramCount++;
    cmdOffSecondary = ch["cmd_off_secondary"].as<std::string>();
  }
  if (ch["main_thermometer_channel_no"]) {
    paramCount++;
    mainThermometerChannelNo = ch["main_thermometer_channel_no"].as<int>();
  } else {
    SUPLA_LOG_ERROR(
        "Channel[%d] config: missing mandatory \"main_thermometer_channel_no\" "
        "parameter",
        channelNumber);
    return false;
  }
  if (ch["aux_thermometer_channel_no"]) {
    paramCount++;
    auxThermometerChannelNo = ch["aux_thermometer_channel_no"].as<int>();
  }

  if (ch["binary_sensor_channel_no"]) {
    paramCount++;
    binarySensorChannelNo = ch["binary_sensor_channel_no"].as<int>();
  }

  auto hvac = new Supla::Control::HvacParsed(
      cmdOn, cmdOff, cmdOnSecondary, cmdOffSecondary);
  hvac->setMainThermometerChannelNo(mainThermometerChannelNo);
  hvac->setAuxThermometerChannelNo(auxThermometerChannelNo);
  if (binarySensorChannelNo >= 0) {
    hvac->setBinarySensorChannelNo(binarySensorChannelNo);
  }
  hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);
  if (hvac->getChannelNumber() != auxThermometerChannelNo) {
    hvac->setAuxThermometerType(SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR);
  }
  hvac->setTemperatureHisteresisMin(20);  // 0.2 degree
  hvac->setTemperatureHisteresisMax(1000);  // 10 degree
  hvac->setTemperatureHeatCoolOffsetMin(200);   // 2 degrees
  hvac->setTemperatureHeatCoolOffsetMax(1000);  // 10 degrees
  hvac->setTemperatureAuxMin(500);  // 5 degrees
  hvac->setTemperatureAuxMax(7500);  // 75 degrees
  hvac->addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);

  hvac->setTemperatureHisteresis(40);

  if (ch["default_function"]) {
    paramCount++;
    std::string function = ch["default_function"].as<std::string>();
    if (function == "heat") {
      hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
      hvac->setDefaultSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
    } else if (function == "cool") {
      hvac->getChannel()->setDefaultFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
      hvac->setDefaultSubfunction(SUPLA_HVAC_SUBFUNCTION_COOL);
    } else if (function == "heat_cool") {
      if (cmdOnSecondary.empty() || cmdOffSecondary.empty()) {
        SUPLA_LOG_ERROR(
            "Channel[%d] config: missing mandatory \"cmd_on_secondary\" or "
            "\"cmd_off_secondary\" parameter when setting \"heat_cool\" "
            "function",
            channelNumber);
        return false;
      }
      hvac->getChannel()->setDefaultFunction(
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
    } else if (function == "dhw") {
      hvac->enableDomesticHotWaterFunctionSupport();
      hvac->getChannel()->setDefaultFunction(
          SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER);
    } else if (function == "diff") {
      hvac->getChannel()->setDefaultFunction(
          SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL);
    }
  }
  return true;
}

bool Supla::LinuxYamlConfig::addThermometerParsed(
    const YAML::Node& ch, int channelNumber, Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding ThremometerParsed", channelNumber);
  auto therm = new Supla::Sensor::ThermometerParsed(parser);
  if (ch[Supla::Parser::Temperature]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Temperature].as<int>();
      therm->setMapping(Supla::Parser::Temperature, index);
    } else {
      std::string key = ch[Supla::Parser::Temperature].as<std::string>();
      therm->setMapping(Supla::Parser::Temperature, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Temperature);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    therm->setMultiplier(Supla::Parser::Temperature, multiplier);
  } else if (ch[Supla::MultiplierTemp]) {
    paramCount++;
    double multiplier = ch[Supla::MultiplierTemp].as<double>();
    therm->setMultiplier(Supla::Parser::Temperature, multiplier);
  }

  auto sensor = therm;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addGeneralPurposeMeasurementParsed(
    const YAML::Node& ch, int channelNumber, Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding GeneralPurposeMeasurement",
                 channelNumber);
  auto gpm = new Supla::Sensor::GeneralPurposeMeasurementParsed(parser);
  if (ch[Supla::Parser::Value]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Value].as<int>();
      gpm->setMapping(Supla::Parser::Value, index);
    } else {
      std::string key = ch[Supla::Parser::Value].as<std::string>();
      gpm->setMapping(Supla::Parser::Value, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Value);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    gpm->setMultiplier(Supla::Parser::Value, multiplier);
  }
  if (ch["default_value_multiplier"]) {
    paramCount++;
    int64_t multiplier =
        std::lround(1000 * ch["default_value_multiplier"].as<double>());
    if (multiplier > INT32_MAX || multiplier < INT32_MIN) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_value_multiplier out of range",
          channelNumber);
      return false;
    }
    gpm->setDefaultValueMultiplier(multiplier);
  }
  if (ch["default_value_divider"]) {
    paramCount++;
    int64_t divider =
        std::lround(1000 * ch["default_value_divider"].as<double>());
    if (divider > INT32_MAX || divider < INT32_MIN) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_value_divider out of range",
          channelNumber);
      return false;
    }
    gpm->setDefaultValueDivider(divider);
  }
  if (ch["default_value_added"]) {
    paramCount++;
    double added = ch["default_value_added"].as<double>();
    gpm->setDefaultValueAdded(std::lround(added * 1000));
  }
  if (ch["default_value_precision"]) {
    paramCount++;
    int precision = ch["default_value_precision"].as<int>();
    if (precision > 4 || precision < 0) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_value_precision out of range",
          channelNumber);
      return false;
    }
    gpm->setDefaultValuePrecision(precision);
  }
  if (ch["default_unit_before_value"]) {
    paramCount++;
    std::string unit = ch["default_unit_before_value"].as<std::string>();
    if (unit.length() > 14) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_unit_before_value too long",
          channelNumber);
      return false;
    }
    gpm->setDefaultUnitBeforeValue(unit.c_str());
  }
  if (ch["default_unit_after_value"]) {
    paramCount++;
    std::string unit = ch["default_unit_after_value"].as<std::string>();
    if (unit.length() > 14) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_unit_after_value too long",
          channelNumber);
      return false;
    }
    gpm->setDefaultUnitAfterValue(unit.c_str());
  }

  auto sensor = gpm;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }
  return true;
}

bool Supla::LinuxYamlConfig::addGeneralPurposeMeterParsed(
    const YAML::Node& ch, int channelNumber, Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding GeneralPurposeMeter",
                 channelNumber);
  auto gpm = new Supla::Sensor::GeneralPurposeMeterParsed(parser);
  if (ch[Supla::Parser::Value]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Value].as<int>();
      gpm->setMapping(Supla::Parser::Value, index);
    } else {
      std::string key = ch[Supla::Parser::Value].as<std::string>();
      gpm->setMapping(Supla::Parser::Value, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Value);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    gpm->setMultiplier(Supla::Parser::Value, multiplier);
  }
  if (ch["default_value_multiplier"]) {
    paramCount++;
    int64_t multiplier =
        std::lround(1000 * ch["default_value_multiplier"].as<double>());
    if (multiplier > INT32_MAX || multiplier < INT32_MIN) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_value_multiplier out of range",
          channelNumber);
      return false;
    }
    gpm->setDefaultValueMultiplier(multiplier);
  }
  if (ch["default_value_divider"]) {
    paramCount++;
    int64_t divider =
        std::lround(1000 * ch["default_value_divider"].as<double>());
    if (divider > INT32_MAX || divider < INT32_MIN) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_value_divider out of range",
          channelNumber);
      return false;
    }
    gpm->setDefaultValueDivider(divider);
  }
  if (ch["default_value_added"]) {
    paramCount++;
    double added = ch["default_value_added"].as<double>();
    gpm->setDefaultValueAdded(std::lround(added * 1000));
  }
  if (ch["default_value_precision"]) {
    paramCount++;
    int precision = ch["default_value_precision"].as<int>();
    if (precision > 4 || precision < 0) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_value_precision out of range",
          channelNumber);
      return false;
    }
    gpm->setDefaultValuePrecision(precision);
  }
  if (ch["default_unit_before_value"]) {
    paramCount++;
    std::string unit = ch["default_unit_before_value"].as<std::string>();
    if (unit.length() > 14) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_unit_before_value too long",
          channelNumber);
      return false;
    }
    gpm->setDefaultUnitBeforeValue(unit.c_str());
  }
  if (ch["default_unit_after_value"]) {
    paramCount++;
    std::string unit = ch["default_unit_after_value"].as<std::string>();
    if (unit.length() > 14) {
      SUPLA_LOG_ERROR(
          "Channel[%d] config: default_unit_after_value too long",
          channelNumber);
      return false;
    }
    gpm->setDefaultUnitAfterValue(unit.c_str());
  }

  auto sensor = gpm;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }
  return true;
}

bool Supla::LinuxYamlConfig::addImpulseCounterParsed(
    const YAML::Node& ch, int channelNumber, Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding ImpulseCounterParsed",
                 channelNumber);
  auto ic = new Supla::Sensor::ImpulseCounterParsed(parser);
  if (ch[Supla::Parser::Counter]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Counter].as<int>();
      ic->setMapping(Supla::Parser::Counter, index);
    } else {
      std::string key = ch[Supla::Parser::Counter].as<std::string>();
      ic->setMapping(Supla::Parser::Counter, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Counter);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    ic->setMultiplier(Supla::Parser::Counter, multiplier);
  }

  auto sensor = ic;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addElectricityMeterParsed(
    const YAML::Node& ch, int channelNumber, Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding ElectricityMeterParsed",
                 channelNumber);
  auto em = new Supla::Sensor::ElectricityMeterParsed(parser);

  // set not phase releated parameters (currently only frequency)
  if (ch[Supla::Parser::Frequency]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Frequency].as<int>();
      em->setMapping(Supla::Parser::Frequency, index);
    } else {
      std::string key = ch[Supla::Parser::Frequency].as<std::string>();
      em->setMapping(Supla::Parser::Frequency, key);
    }
    if (ch[Supla::Multiplier]) {
      double multiplier = ch[Supla::Multiplier].as<double>();
      em->setMultiplier(Supla::Parser::Frequency, multiplier);
    }
  }

  const std::map<std::string, int> phases = {
      {"phase_1", 1}, {"phase_2", 2}, {"phase_3", 3}};

  for (auto i : phases) {
    if (ch[i.first]) {
      paramCount++;
      auto phaseParameters = ch[i.first];
      int phaseId = i.second;

      for (auto param : phaseParameters) {
        std::string paramName;
        for (const std::string& name : {"voltage",
                                        "current",
                                        "fwd_act_energy",
                                        "rvr_act_energy",
                                        "fwd_react_energy",
                                        "rvr_react_energy",
                                        "power_active",
                                        "rvr_power_active",
                                        "power_reactive",
                                        "power_apparent",
                                        "phase_angle",
                                        "power_factor"}) {
          if (param[name]) {
            paramName = name + "_" + std::to_string(phaseId);
            if (parser->isBasedOnIndex()) {
              int index = param[name].as<int>();
              em->setMapping(paramName, index);
            } else {
              std::string key = param[name].as<std::string>();
              em->setMapping(paramName, key);
            }
            if (param[Supla::Multiplier]) {
              double multiplier = param[Supla::Multiplier].as<double>();
              em->setMultiplier(paramName, multiplier);
            }
          }
        }
      }
    }
  }

  auto sensor = em;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addBinaryParsed(const YAML::Node& ch,
                                             int channelNumber,
                                             Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding BinaryParsed", channelNumber);
  auto binary = new Supla::Sensor::BinaryParsed(parser);

  if (!addStateParser(ch, binary, parser, true)) {
    return false;
  }

  auto sensor = binary;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  if (!addActionTriggerActions(ch, sensor, false)) {
    return false;
  }

  return true;
}

Supla::Parser::Parser* Supla::LinuxYamlConfig::addParser(
    const YAML::Node& parser, Supla::Source::Source* src) {
  Supla::Parser::Parser* prs = nullptr;
  if (parser["use"]) {
    std::string use = parser["use"].as<std::string>();
    if (parserNames.count(use)) {
      prs = parsers[parserNames[use]];
    }
    if (!prs) {
      SUPLA_LOG_ERROR("Config: can't find parser with \"name\"=\"%s\"",
                      use.c_str());
      return nullptr;
    }
    if (parser["name"]) {
      SUPLA_LOG_ERROR(
          "Config: can't use \"name\" for parser with \"use\" parameter");
      return nullptr;
    }
    return prs;
  }

  if (parser["name"]) {
    std::string name = parser["name"].as<std::string>();
    parserNames[name] = parserCount;
  }

  if (!src) {
    SUPLA_LOG_ERROR("Config: parser used without source");
    return nullptr;
  }

  if (parser["type"]) {
    std::string type = parser["type"].as<std::string>();
    if (type == "Simple") {
      prs = new Supla::Parser::Simple(src);
    } else if (type == "Json") {
      prs = new Supla::Parser::Json(src);
    } else {
      SUPLA_LOG_ERROR("Config: unknown parser type \"%s\"", type.c_str());
      return nullptr;
    }
    if (parser["refresh_time_ms"]) {
      int timeMs = parser["refresh_time_ms"].as<int>();
      prs->setRefreshTime(timeMs);
    }
  } else {
    SUPLA_LOG_ERROR("Config: type not defined for parser");
    return nullptr;
  }

  parsers[parserCount] = prs;
  parserCount++;
  return prs;
}

Supla::Source::Source* Supla::LinuxYamlConfig::addSource(
    const YAML::Node& source) {
  Supla::Source::Source* src = nullptr;
  if (source["use"]) {
    std::string use = source["use"].as<std::string>();
    if (sourceNames.count(use)) {
      src = sources[sourceNames[use]];
    }
    if (!src) {
      SUPLA_LOG_ERROR("Config: can't find source with \"name\"=\"%s\"",
                      use.c_str());
      return nullptr;
    }
    if (source["name"]) {
      SUPLA_LOG_ERROR(
          "Config: can't use \"name\" for source with \"use\" parameter");
      return nullptr;
    }
    return src;
  }

  if (source["name"]) {
    std::string name = source["name"].as<std::string>();
    sourceNames[name] = sourceCount;
  }

  if (source["type"]) {
    std::string type = source["type"].as<std::string>();
    if (type == "File") {
      std::string fileName = source["file"].as<std::string>();
      int expirationTimeSec = 10 * 60;
      if (source["expiration_time_sec"]) {
        expirationTimeSec = source["expiration_time_sec"].as<int>();
      }
      src = new Supla::Source::File(fileName.c_str(), expirationTimeSec);
    } else if (type == "Cmd") {
      std::string cmd = source["command"].as<std::string>();
      src = new Supla::Source::Cmd(cmd.c_str());
    } else {
      SUPLA_LOG_ERROR("Config: unknown source type \"%s\"", type.c_str());
      return nullptr;
    }

  } else {
    SUPLA_LOG_ERROR("Config: type not defined for source");
    return nullptr;
  }

  sources[sourceCount] = src;
  sourceCount++;

  return src;
}

std::string Supla::LinuxYamlConfig::getStateFilesPath() {
  std::string path;
  try {
    if (config["state_files_path"]) {
      path = config["state_files_path"].as<std::string>();
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Config file YAML error: %s", ex.what());
  }
  if (path.empty()) {
    SUPLA_LOG_DEBUG("Config: missing state files path - using default");
    path = "var/lib/supla-device";
  }
  return path;
}

void Supla::LinuxYamlConfig::loadGuidAuthFromPath(const std::string& path) {
  try {
    std::string file = path + Supla::GuidAuthFileName;
    SUPLA_LOG_INFO("GUID and AUTHKEY: loading from file %s", file.c_str());
    auto guidAuthYaml = YAML::LoadFile(file);
    std::string guidHex;
    std::string authHex;
    if (guidAuthYaml[Supla::GuidKey]) {
      guidHex = guidAuthYaml[Supla::GuidKey].as<std::string>();
      if (guidHex.length() != SUPLA_GUID_HEXSIZE - 1) {
        SUPLA_LOG_WARNING("GUID: invalid guid value length");
        guidHex = "";
      }
      guid = guidHex;
    } else {
      SUPLA_LOG_WARNING("GUID: missing guid key in yaml file");
    }
    if (guidAuthYaml[Supla::AuthKeyKey]) {
      authHex = guidAuthYaml[Supla::AuthKeyKey].as<std::string>();
      if (authHex.length() != SUPLA_AUTHKEY_HEXSIZE - 1) {
        SUPLA_LOG_WARNING("AUTHKEY: invalid authkey value length");
        authHex = "";
      }
      authkey = authHex;
    } else {
      SUPLA_LOG_WARNING("AUTHKEY: missing authkey key in yaml file");
    }
  } catch (const YAML::Exception& ex) {
    SUPLA_LOG_ERROR("Guid/auth file YAML error: %s", ex.what());
  }
}

bool Supla::LinuxYamlConfig::saveGuidAuth(const std::string& path) {
  if (!std::filesystem::exists(path)) {
    std::error_code err;
    if (!std::filesystem::create_directories(path, err)) {
      SUPLA_LOG_WARNING("Config: failed to create folder for state files");
      return false;
    }
  }
  YAML::Node outputYaml;
  outputYaml[Supla::GuidKey] = guid;
  outputYaml[Supla::AuthKeyKey] = authkey;

  std::ofstream out(path + Supla::GuidAuthFileName);
  out << outputYaml;
  out.close();
  if (out.fail()) {
    SUPLA_LOG_ERROR("Config: failed to write guid/authkey to file");
    return false;
  }

  return true;
}

bool Supla::LinuxYamlConfig::isConfigModeSupported() {
  return false;
}

bool Supla::LinuxYamlConfig::addThermHygroMeterParsed(
    const YAML::Node& ch, int channelNumber, Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding ThermHygroMeterParsed",
                 channelNumber);
  auto thermHumi = new Supla::Sensor::ThermHygroMeterParsed(parser);
  if (ch[Supla::Parser::Humidity]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Humidity].as<int>();
      thermHumi->setMapping(Supla::Parser::Humidity, index);
    } else {
      std::string key = ch[Supla::Parser::Humidity].as<std::string>();
      thermHumi->setMapping(Supla::Parser::Humidity, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Humidity);
    return false;
  }

  if (ch[Supla::MultiplierHumi]) {
    paramCount++;
    double multiplier = ch[Supla::MultiplierHumi].as<double>();
    thermHumi->setMultiplier(Supla::Parser::Humidity, multiplier);
  }

  if (ch[Supla::Parser::Temperature]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Temperature].as<int>();
      thermHumi->setMapping(Supla::Parser::Temperature, index);
    } else {
      std::string key = ch[Supla::Parser::Temperature].as<std::string>();
      thermHumi->setMapping(Supla::Parser::Temperature, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Temperature);
    return false;
  }

  if (ch[Supla::MultiplierTemp]) {
    paramCount++;
    double multiplier = ch[Supla::MultiplierTemp].as<double>();
    thermHumi->setMultiplier(Supla::Parser::Temperature, multiplier);
  }

  auto sensor = thermHumi;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addHumidityParsed(const YAML::Node& ch,
                                               int channelNumber,
                                               Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding HumidityParsed", channelNumber);
  auto humi = new Supla::Sensor::HumidityParsed(parser);
  if (ch[Supla::Parser::Humidity]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Humidity].as<int>();
      humi->setMapping(Supla::Parser::Humidity, index);
    } else {
      std::string key = ch[Supla::Parser::Humidity].as<std::string>();
      humi->setMapping(Supla::Parser::Humidity, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Humidity);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    humi->setMultiplier(Supla::Parser::Humidity, multiplier);
  }

  auto sensor = humi;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addPressureParsed(const YAML::Node& ch,
                                               int channelNumber,
                                               Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding PressureParsed", channelNumber);
  auto pressure = new Supla::Sensor::PressureParsed(parser);
  if (ch[Supla::Parser::Pressure]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Pressure].as<int>();
      pressure->setMapping(Supla::Parser::Pressure, index);
    } else {
      std::string key = ch[Supla::Parser::Pressure].as<std::string>();
      pressure->setMapping(Supla::Parser::Pressure, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Pressure);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    pressure->setMultiplier(Supla::Parser::Pressure, multiplier);
  }

  auto sensor = pressure;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addWindParsed(const YAML::Node& ch,
                                           int channelNumber,
                                           Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding WindParsed", channelNumber);
  auto wind = new Supla::Sensor::WindParsed(parser);
  if (ch[Supla::Parser::Wind]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Wind].as<int>();
      wind->setMapping(Supla::Parser::Wind, index);
    } else {
      std::string key = ch[Supla::Parser::Wind].as<std::string>();
      wind->setMapping(Supla::Parser::Wind, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Wind);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    wind->setMultiplier(Supla::Parser::Wind, multiplier);
  }

  auto sensor = wind;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addRainParsed(const YAML::Node& ch,
                                           int channelNumber,
                                           Supla::Parser::Parser* parser) {
  SUPLA_LOG_INFO("Channel[%d] config: adding RainParsed", channelNumber);
  auto rain = new Supla::Sensor::RainParsed(parser);
  if (ch[Supla::Parser::Rain]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::Rain].as<int>();
      rain->setMapping(Supla::Parser::Rain, index);
    } else {
      std::string key = ch[Supla::Parser::Rain].as<std::string>();
      rain->setMapping(Supla::Parser::Rain, key);
    }
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: missing \"%s\" parameter",
                    channelNumber,
                    Supla::Parser::Rain);
    return false;
  }
  if (ch[Supla::Multiplier]) {
    paramCount++;
    double multiplier = ch[Supla::Multiplier].as<double>();
    rain->setMultiplier(Supla::Parser::Rain, multiplier);
  }

  auto sensor = rain;
  if (ch[Supla::Sensor::BatteryLevel]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Sensor::BatteryLevel].as<int>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, index);
    } else {
      std::string key = ch[Supla::Sensor::BatteryLevel].as<std::string>();
      sensor->setMapping(Supla::Sensor::BatteryLevel, key);
    }
  }
  if (ch[Supla::Sensor::MultiplierBatteryLevel]) {
    paramCount++;
    double multiplier = ch[Supla::Sensor::MultiplierBatteryLevel].as<double>();
    sensor->setMultiplier(Supla::Sensor::BatteryLevel, multiplier);
  }

  return true;
}

bool Supla::LinuxYamlConfig::addStateParser(
    const YAML::Node& ch,
    Supla::Sensor::SensorParsedBase* sensor,
    Supla::Parser::Parser* parser,
    bool mandatory) {
  if (parser == nullptr && ch[Supla::Parser::State]) {
    SUPLA_LOG_ERROR("Channel config: missing parser");
    return false;
  }

  if (ch[Supla::Parser::State]) {
    paramCount++;
    if (parser->isBasedOnIndex()) {
      int index = ch[Supla::Parser::State].as<int>();
      sensor->setMapping(Supla::Parser::State, index);
    } else {
      std::string key = ch[Supla::Parser::State].as<std::string>();
      sensor->setMapping(Supla::Parser::State, key);
    }
    if (ch[Supla::Parser::StateOnValues]) {
      paramCount++;
      sensor->setOnValues(
          ch[Supla::Parser::StateOnValues].as<std::vector<int>>());
    }
  } else {
    if (mandatory) {
      SUPLA_LOG_ERROR("Channel config: missing \"%s\" parameter",
                      Supla::Parser::State);
      return false;
    }
  }
  return true;
}

bool Supla::LinuxYamlConfig::addActionTriggerActions(
    const YAML::Node& ch,
    Supla::Sensor::SensorParsedBase* sensor,
    bool mandatory) {
  if (mandatory && !ch[Supla::Parser::ActionTrigger]) {
    SUPLA_LOG_ERROR("Channel config: mandatory \"%s\" parameter missing",
                    Supla::Parser::ActionTrigger);
    return false;
  }

  if (ch[Supla::Parser::ActionTrigger]) {
    paramCount++;
    bool atNameFound = false;
    for (const auto& el : ch[Supla::Parser::ActionTrigger]) {
      if (el["on_state"]) {
        if (!sensor->addAtOnState(el["on_state"].as<std::vector<int>>())) {
          return false;
        }
      } else if (el["on_value"]) {
        if (!sensor->addAtOnValue(el["on_value"].as<std::vector<int>>())) {
          return false;
        }
      } else if (el["on_value_change"]) {
        if (!sensor->addAtOnValueChange(
                el["on_value_change"].as<std::vector<int>>())) {
          return false;
        }
      } else if (el["on_state_change"]) {
        if (!sensor->addAtOnStateChange(
                el["on_state_change"].as<std::vector<int>>())) {
          return false;
        }
      } else if (el["use"]) {
        SUPLA_LOG_INFO("Channel config: using action trigger \"%s\"",
                       el["use"].as<std::string>().c_str());
        sensor->setAtName(el["use"].as<std::string>());
        atNameFound = true;
      }
    }
    if (!atNameFound) {
      SUPLA_LOG_ERROR(
          "Channel config: mandatory \"use\" parameter missing with "
          "ActionTrigger name");
      return false;
    }
  }
  return true;
}

bool Supla::LinuxYamlConfig::addActionTriggerParsed(const YAML::Node& ch,
                                                    int channelNumber) {
  SUPLA_LOG_INFO("Channel[%d] config: adding ActionTriggerParsed",
                 channelNumber);
  if (ch["name"]) {
    paramCount++;
    new Supla::Control::ActionTriggerParsed(ch["name"].as<std::string>());
  } else {
    SUPLA_LOG_ERROR("Channel[%d] config: mandatory \"name\" parameter missing",
                    channelNumber);
    return false;
  }
  return true;
}
