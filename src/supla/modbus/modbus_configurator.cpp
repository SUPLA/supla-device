/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "modbus_configurator.h"

#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/storage/config_tags.h>
#include <supla/device/remote_device_config.h>

using Supla::Modbus::Configurator;

bool Supla::Modbus::Config::operator==(const Config &other) const {
  return (role == other.role && modbusAddress == other.modbusAddress &&
          serial.mode == other.serial.mode &&
          serial.baudrate == other.serial.baudrate &&
          serial.stopBits == other.serial.stopBits &&
          network.mode == other.network.mode &&
          network.port == other.network.port);
}

bool Supla::Modbus::Config::operator!=(const Config &other) const {
  return !(*this == other);
}

Supla::Modbus::Config &Supla::Modbus::Config::operator=(
    const Supla::Modbus::Config &other) {
  role = other.role;
  modbusAddress = other.modbusAddress;
  serial.mode = other.serial.mode;
  serial.baudrate = other.serial.baudrate;
  serial.stopBits = other.serial.stopBits;
  network.mode = other.network.mode;
  network.port = other.network.port;
  return *this;
}

Supla::Modbus::ConfigProperties::ConfigProperties() {
  protocol.master = 0;
  protocol.slave = 0;
  protocol.rtu = 0;
  protocol.ascii = 0;
  protocol.tcp = 0;
  protocol.udp = 0;
  baudrate.b4800 = 0;
  baudrate.b9600 = 0;
  baudrate.b19200 = 0;
  baudrate.b38400 = 0;
  baudrate.b57600 = 0;
  baudrate.b115200 = 0;
  stopBits.one = 0;
  stopBits.oneAndHalf = 0;
  stopBits.two = 0;
}

bool Supla::Modbus::ConfigProperties::operator==(
    const ModbusConfigProperties &other) const {
  return (protocol.master == other.Protocol.Master &&
          protocol.slave == other.Protocol.Slave &&
          protocol.rtu == other.Protocol.Rtu &&
          protocol.ascii == other.Protocol.Ascii &&
          protocol.tcp == other.Protocol.Tcp &&
          protocol.udp == other.Protocol.Udp &&
          baudrate.b4800 == other.Baudrate.B4800 &&
          baudrate.b9600 == other.Baudrate.B9600 &&
          baudrate.b19200 == other.Baudrate.B19200 &&
          baudrate.b38400 == other.Baudrate.B38400 &&
          baudrate.b57600 == other.Baudrate.B57600 &&
          baudrate.b115200 == other.Baudrate.B115200 &&
          stopBits.one == other.StopBits.One &&
          stopBits.oneAndHalf == other.StopBits.OneAndHalf &&
          stopBits.two == other.StopBits.Two);
}

bool Supla::Modbus::ConfigProperties::operator==(
    const ConfigProperties &other) const {
  return (protocol.master == other.protocol.master &&
          protocol.slave == other.protocol.slave &&
          protocol.rtu == other.protocol.rtu &&
          protocol.ascii == other.protocol.ascii &&
          protocol.tcp == other.protocol.tcp &&
          protocol.udp == other.protocol.udp &&
          baudrate.b4800 == other.baudrate.b4800 &&
          baudrate.b9600 == other.baudrate.b9600 &&
          baudrate.b19200 == other.baudrate.b19200 &&
          baudrate.b38400 == other.baudrate.b38400 &&
          baudrate.b57600 == other.baudrate.b57600 &&
          baudrate.b115200 == other.baudrate.b115200 &&
          stopBits.one == other.stopBits.one &&
          stopBits.oneAndHalf == other.stopBits.oneAndHalf &&
          stopBits.two == other.stopBits.two);
}

bool Supla::Modbus::ConfigProperties::operator!=(
    const ModbusConfigProperties &other) const {
  return !(*this == other);
}

bool Supla::Modbus::ConfigProperties::operator!=(
    const ConfigProperties &other) const {
    return !(*this == other);
}

bool Supla::Modbus::Config::validateAndFix(const ConfigProperties &properties) {
  bool changed = false;
  if (role == Role::Slave && !properties.protocol.slave) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid modbus role %d", role);
    role = Role::NotSet;
    changed = true;
  }
  if (role == Role::Master && !properties.protocol.master) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid modbus role %d", role);
    role = Role::NotSet;
    changed = true;
  }
  if (serial.mode == ModeSerial::Rtu && !properties.protocol.rtu) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid serial mode %d",
                      serial.mode);
    serial.mode = ModeSerial::Disabled;
    changed = true;
  }
  if (serial.mode == ModeSerial::Ascii && !properties.protocol.ascii) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid serial mode %d",
                      serial.mode);
    serial.mode = ModeSerial::Disabled;
    changed = true;
  }
  if (network.mode == ModeNetwork::Tcp && !properties.protocol.tcp) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid network mode %d",
                      network.mode);
    network.mode = ModeNetwork::Disabled;
    changed = true;
  }
  if (network.mode == ModeNetwork::Udp && !properties.protocol.udp) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid network mode %d",
                      network.mode);
    network.mode = ModeNetwork::Disabled;
    changed = true;
  }

  if (modbusAddress < 1 || modbusAddress > 247) {
    SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid modbus address %d",
                      modbusAddress);
    modbusAddress = 1;
    changed = true;
  }
  switch (serial.baudrate) {
    case 9600:
    case 19200: {
      // 9600 and 19200 are mandatory, so we always accept them
      break;
    }
    case 4800: {
      if (!properties.baudrate.b4800) {
        serial.baudrate = 19200;
        changed = true;
      }
      break;
    }
    case 38400: {
      if (!properties.baudrate.b38400) {
        serial.baudrate = 19200;
        changed = true;
      }
      break;
    }
    case 57600: {
      if (!properties.baudrate.b57600) {
        serial.baudrate = 19200;
        changed = true;
      }
      break;
    }
    case 115200: {
      if (!properties.baudrate.b115200) {
        serial.baudrate = 19200;
        changed = true;
      }
      break;
    }
    default: {
      SUPLA_LOG_WARNING("RemoteDeviceConfig: invalid serial baudrate %d",
                        serial.baudrate);
      serial.baudrate = 19200;
      changed = true;
    }
  }
  return changed;
}

Configurator::Configurator() {}

void Configurator::onInit() {
  initDone = true;
}


void Configurator::onLoadConfig(SuplaDeviceClass *) {
  Supla::Device::RemoteDeviceConfig::SetModbusProperties(getProperties());
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    Supla::Modbus::Config tempConfig = {};
    if (cfg->getBlob(Supla::ConfigTag::ModbusCfgTag,
                     reinterpret_cast<char *>(&tempConfig),
                     sizeof(tempConfig))) {
      if (config != tempConfig) {
        config = tempConfig;
        configChanged = true;
        SUPLA_LOG_INFO("Modbus config loaded from storage");
      }
    } else {
      // store default config
      cfg->setBlob(Supla::ConfigTag::ModbusCfgTag,
                   reinterpret_cast<const char *>(&config),
                   sizeof(config));
      cfg->saveWithDelay(5000);
    }
  }

  if (this->config.validateAndFix(getProperties())) {
    storeConfig(true);
  }
  printConfig();
}

void Configurator::onDeviceConfigChange(uint64_t fieldBit) {
  if (fieldBit == SUPLA_DEVICE_CONFIG_FIELD_MODBUS) {
    // reload config
    SUPLA_LOG_DEBUG("Modbus: reload config");
    onLoadConfig(nullptr);
  }
}

bool Configurator::isNetworkModeEnabled() const {
  if (config.role != Supla::Modbus::Role::NotSet) {
    return (config.network.mode != Supla::Modbus::ModeNetwork::Disabled);
  }
  return false;
}

bool Configurator::isSerialModeEnabled() const {
  if (config.role != Supla::Modbus::Role::NotSet) {
    return (config.serial.mode != Supla::Modbus::ModeSerial::Disabled);
  }
  return false;
}

bool Configurator::isModbusDisabled() const {
  if (config.role != Supla::Modbus::Role::NotSet) {
    return (config.serial.mode == Supla::Modbus::ModeSerial::Disabled &&
            config.network.mode == Supla::Modbus::ModeNetwork::Disabled);
  }
  return true;
}

bool Configurator::isMaster() const {
  return config.role == Supla::Modbus::Role::Master;
}

bool Configurator::isSlave() const {
  return config.role == Supla::Modbus::Role::Slave;
}

bool Configurator::isRtuMode() const {
  return (config.serial.mode == Supla::Modbus::ModeSerial::Rtu);
}

bool Configurator::isAsciiMode() const {
  return (config.serial.mode == Supla::Modbus::ModeSerial::Ascii);
}

void Configurator::setConfig(const Supla::Modbus::Config &config) {
  if (this->config != config) {
    this->config = config;
    configChanged = true;

    this->config.validateAndFix(getProperties());

    printConfig();
    storeConfig(true);
  }
}

void Configurator::storeConfig(bool local) const {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    cfg->setBlob(Supla::ConfigTag::ModbusCfgTag,
                 reinterpret_cast<const char *>(&config),
                 sizeof(config));
    cfg->saveWithDelay(5000);
    if (initDone && local) {
      cfg->setDeviceConfigChangeFlag();
    }
  }
}

const Supla::Modbus::Config &Configurator::getConfig() const {
  return config;
}

const Supla::Modbus::ConfigProperties &Configurator::getProperties() const {
  return properties;
}

void Configurator::setProperties(
    const Supla::Modbus::ConfigProperties &newProperties) {
  properties = newProperties;
}

void Configurator::printConfig() const {
  SUPLA_LOG_INFO(
      "Modbus config: %s, address: %d, serial mode: %s, baudrate: %d, "
      "stop bits: %s, network mode: %s, port: %d",
      config.role == Supla::Modbus::Role::NotSet   ? "disabled"
      : config.role == Supla::Modbus::Role::Master ? "master"
                                                   : "slave",
      config.modbusAddress,
      config.serial.mode == Supla::Modbus::ModeSerial::Disabled
          ? "disabled"
          : config.serial.mode == Supla::Modbus::ModeSerial::Rtu
                ? "RTU"
                : "ASCII",
      config.serial.baudrate,
      config.serial.stopBits == Supla::Modbus::SerialStopBits::One ? "1"
      : config.serial.stopBits == Supla::Modbus::SerialStopBits::OneAndHalf
            ? "1.5"
            : "2",
      config.network.mode == Supla::Modbus::ModeNetwork::Disabled
          ? "disabled"
          : config.network.mode == Supla::Modbus::ModeNetwork::Tcp
                ? "TCP"
                : "UDP",
      config.network.port);
}
