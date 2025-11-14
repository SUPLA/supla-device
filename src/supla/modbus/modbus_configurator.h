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

#ifndef SRC_SUPLA_MODBUS_MODBUS_CONFIGURATOR_H_
#define SRC_SUPLA_MODBUS_MODBUS_CONFIGURATOR_H_

#include <supla/element.h>

namespace Supla {

namespace Modbus {

enum class Role : uint8_t {
  NotSet = MODBUS_ROLE_NOT_SET,
  Master = MODBUS_ROLE_MASTER,
  Slave = MODBUS_ROLE_SLAVE
};

enum class ModeSerial : uint8_t {
  Disabled = MODBUS_SERIAL_MODE_DISABLED,
  Rtu = MODBUS_SERIAL_MODE_RTU,
  Ascii = MODBUS_SERIAL_MODE_ASCII
};

enum class ModeNetwork : uint8_t {
  Disabled = MODBUS_NETWORK_MODE_DISABLED,
  Tcp = MODBUS_NETWORK_MODE_TCP,
  Udp = MODBUS_NETWORK_MODE_UDP
};

enum class SerialStopBits : uint8_t {
  One = MODBUS_SERIAL_STOP_BITS_ONE,
  OneAndHalf = MODBUS_SERIAL_STOP_BITS_ONE_AND_HALF,
  Two = MODBUS_SERIAL_STOP_BITS_TWO
};

#pragma pack(push, 1)
struct SerialConfig {
  ModeSerial mode = ModeSerial::Disabled;
  int baudrate = 19200;
  SerialStopBits stopBits = SerialStopBits::One;
};

struct NetworkConfig {
  ModeNetwork mode = ModeNetwork::Disabled;
  int port = 502;
};

struct ConfigProperties {
  struct {
    uint8_t master: 1;
    uint8_t slave: 1;
    uint8_t rtu: 1;
    uint8_t ascii: 1;
    uint8_t tcp: 1;
    uint8_t udp: 1;
  } protocol;
  struct {
    uint8_t b4800 : 1;
    uint8_t b9600 : 1;   // modbus mandatory
    uint8_t b19200 : 1;  // modbus mandatory
    uint8_t b38400 : 1;
    uint8_t b57600 : 1;
    uint8_t b115200 : 1;
  } baudrate;
  struct {
    uint8_t one: 1;
    uint8_t oneAndHalf: 1;
    uint8_t two: 1;
  } stopBits;

  ConfigProperties();
  bool operator==(const ConfigProperties &other) const;
  bool operator!=(const ConfigProperties &other) const;
  bool operator==(const ModbusConfigProperties &other) const;
  bool operator!=(const ModbusConfigProperties &other) const;
};

struct Config {
  Role role = Role::NotSet;
  uint8_t modbusAddress = 1;  // only for slave
  uint16_t slaveTimeoutMs = 0;
  SerialConfig serial;
  NetworkConfig network;

  bool operator==(const Config &other) const;
  bool operator!=(const Config &other) const;
  Config &operator=(const Config &other);
  bool validateAndFix(const ConfigProperties &properties);
};

#pragma pack(pop)

class Configurator : public Supla::Element {
 public:
  Configurator();

  void onLoadConfig(SuplaDeviceClass *) override;
  void onDeviceConfigChange(uint64_t fieldBit) override;
  void onInit() override;

  bool isNetworkModeEnabled() const;
  bool isSerialModeEnabled() const;
  bool isModbusDisabled() const;
  bool isMaster() const;
  bool isSlave() const;
  bool isRtuMode() const;
  bool isAsciiMode() const;

  void setConfig(const Supla::Modbus::Config &config);
  void storeConfig(bool local = false) const;
  void printConfig() const;
  const Supla::Modbus::Config &getConfig() const;
  const Supla::Modbus::ConfigProperties &getProperties() const;
  void setProperties(const Supla::Modbus::ConfigProperties &newProperties);

 protected:
  bool configChanged = false;
  bool initDone = false;
  Supla::Modbus::Config config;
  Supla::Modbus::ConfigProperties properties;
};

}  // namespace Modbus
}  // namespace Supla

#endif  // SRC_SUPLA_MODBUS_MODBUS_CONFIGURATOR_H_
