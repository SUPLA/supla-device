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

#ifndef ARDUINO_ARCH_AVR
#include "modbus_parameters.h"

#include <string.h>
#include <supla/modbus/modbus_configurator.h>
#include <supla/network/web_sender.h>
#include <supla/tools.h>

using Supla::Html::ModbusParameters;

namespace {
const char ModbusRoleTag[] = "mb_role";
const char ModbusAddressTag[] = "mb_address";
const char ModbusPortTag[] = "mb_port";
const char ModbusTimeoutTag[] = "mb_timeout";
const char ModbusBaudrateTag[] = "mb_baudrate";
const char ModbusStopBitsTag[] = "mb_stopbits";
const char ModbusSerialModeTag[] = "mb_serial_mode";
const char ModbusNetworkModeTag[] = "mb_network_mode";

}  // namespace

ModbusParameters::ModbusParameters(Supla::Modbus::Configurator* modbus)
    : HtmlElement(HTML_SECTION_NETWORK), modbus(modbus) {
}

ModbusParameters::~ModbusParameters() {
}

void ModbusParameters::setModbusPtr(Supla::Modbus::Configurator* modbus) {
  this->modbus = modbus;
}

void ModbusParameters::onProcessingEnd() {
  if (configChanged) {
    modbus->setConfig(config);
    configChanged = false;
  }
}

void ModbusParameters::send(Supla::WebSender* sender) {
  config = modbus->getConfig();
  auto properties = modbus->getProperties();

  auto emitField = [&](const char* id, const char* label, auto&& render) {
    sender->formField([&]() {
      sender->labelFor(id, label);
      sender->tag("div").body([&]() { render(); });
    });
  };

  auto emitSelectField = [&](const char* id, const char* label, auto&& render) {
    emitField(id, label, [&]() {
      auto select = sender->selectTag(id, id);
      select.body([&]() { render(); });
    });
  };

  sender->tag("h3").body("Modbus Settings");

  emitSelectField(ModbusRoleTag, "Modbus mode", [&]() {
    sender->selectOption(
        0, "Disabled", config.role == Supla::Modbus::Role::NotSet);
    if (properties.protocol.master) {
      sender->selectOption(
          1, "Master (client)", config.role == Supla::Modbus::Role::Master);
    }
    if (properties.protocol.slave) {
      sender->selectOption(
          2, "Slave (server)", config.role == Supla::Modbus::Role::Slave);
    }
  });

  if (properties.protocol.slave) {
    emitField(ModbusAddressTag, "Modbus address (only for Slave)", [&]() {
      sender->numberInput(ModbusAddressTag,
                          {
                              .min = 1,
                              .max = 247,
                              .value = config.modbusAddress,
                              .step = 1,
                          });
    });
  }

  if (properties.protocol.rtu || properties.protocol.ascii) {
    emitSelectField(ModbusSerialModeTag, "Modbus serial mode", [&]() {
      sender->selectOption(
          0,
          "Disabled",
          config.serial.mode == Supla::Modbus::ModeSerial::Disabled);
      if (properties.protocol.rtu) {
        sender->selectOption(
            1, "RTU", config.serial.mode == Supla::Modbus::ModeSerial::Rtu);
      }
      if (properties.protocol.ascii) {
        sender->selectOption(
            2, "ASCII", config.serial.mode == Supla::Modbus::ModeSerial::Ascii);
      }
    });
    emitSelectField(ModbusBaudrateTag, "Baudrate", [&]() {
      if (properties.baudrate.b4800) {
        sender->selectOption(4800, "4800", config.serial.baudrate == 4800);
      }
      if (properties.baudrate.b9600) {
        sender->selectOption(9600, "9600", config.serial.baudrate == 9600);
      }
      if (properties.baudrate.b19200) {
        sender->selectOption(19200, "19200", config.serial.baudrate == 19200);
      }
      if (properties.baudrate.b38400) {
        sender->selectOption(38400, "38400", config.serial.baudrate == 38400);
      }
      if (properties.baudrate.b57600) {
        sender->selectOption(57600, "57600", config.serial.baudrate == 57600);
      }
      if (properties.baudrate.b115200) {
        sender->selectOption(
            115200, "115200", config.serial.baudrate == 115200);
      }
    });
    emitSelectField(ModbusStopBitsTag, "Stop bits", [&]() {
      if (properties.stopBits.one) {
        sender->selectOption(
            0,
            "1",
            config.serial.stopBits == Supla::Modbus::SerialStopBits::One);
      }
      if (properties.stopBits.oneAndHalf) {
        sender->selectOption(1,
                             "1.5",
                             config.serial.stopBits ==
                                 Supla::Modbus::SerialStopBits::OneAndHalf);
      }
      if (properties.stopBits.two) {
        sender->selectOption(
            2,
            "2",
            config.serial.stopBits == Supla::Modbus::SerialStopBits::Two);
      }
    });
  }

  if (properties.protocol.tcp || properties.protocol.udp) {
    emitSelectField(ModbusNetworkModeTag, "Modbus network mode", [&]() {
      sender->selectOption(
          0,
          "Disabled",
          config.network.mode == Supla::Modbus::ModeNetwork::Disabled);
      if (properties.protocol.tcp) {
        sender->selectOption(
            1, "TCP", config.network.mode == Supla::Modbus::ModeNetwork::Tcp);
      }
      if (properties.protocol.udp) {
        sender->selectOption(
            2, "UDP", config.network.mode == Supla::Modbus::ModeNetwork::Udp);
      }
    });
  }
}

bool ModbusParameters::handleResponse(const char* key, const char* value) {
  if (strcmp(key, ModbusRoleTag) == 0) {
    int val = 0;
    if (strnlen(value, 10) > 0) {
      val = stringToInt(value);
      if (val > 2) {
        val = 0;
      }
    }
    config.role = static_cast<Supla::Modbus::Role>(val);
    configChanged = true;
    return true;
  } else if (strcmp(key, ModbusAddressTag) == 0) {
    int val = 0;
    if (strnlen(value, 10) > 0) {
      val = stringToInt(value);
      if (val > 255) {
        val = 0;
      }
      if (val < 1) {
        val = 1;
      }
    }
    config.modbusAddress = val;
    configChanged = true;
    return true;
  } else if (strcmp(key, ModbusSerialModeTag) == 0) {
    int val = 0;
    if (strnlen(value, 10) > 0) {
      val = stringToInt(value);
      if (val > 2 || val < 0) {
        val = 0;
      }
    }
    config.serial.mode = static_cast<Supla::Modbus::ModeSerial>(val);
    configChanged = true;
    return true;
  } else if (strcmp(key, ModbusNetworkModeTag) == 0) {
    int val = 0;
    if (strnlen(value, 10) > 0) {
      val = stringToInt(value);
      if (val > 2 || val < 0) {
        val = 0;
      }
    }
    config.network.mode = static_cast<Supla::Modbus::ModeNetwork>(val);
    configChanged = true;
    return true;
  } else if (strcmp(key, ModbusBaudrateTag) == 0) {
    int val = 0;
    if (strnlen(value, 10) > 0) {
      val = stringToInt(value);
      if (val > 115200 || val < 0) {
        val = 0;
      }
    }
    config.serial.baudrate = val;
    configChanged = true;
    return true;
  } else if (strcmp(key, ModbusStopBitsTag) == 0) {
    int val = 0;
    if (strnlen(value, 10) > 0) {
      val = stringToInt(value);
      if (val > 2 || val < 0) {
        val = 0;
      }
    }
    config.serial.stopBits = static_cast<Supla::Modbus::SerialStopBits>(val);
    configChanged = true;
    return true;
  }

  return false;
}

#endif  // ARDUINO_ARCH_AVR
