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

#include "modbus_parameters.h"

#include <supla/network/web_sender.h>
#include <string.h>
#include <supla/tools.h>
#include <supla/modbus/modbus_configurator.h>

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

ModbusParameters::ModbusParameters(Supla::Modbus::Configurator *modbus)
    : HtmlElement(HTML_SECTION_NETWORK), modbus(modbus) {
}

ModbusParameters::~ModbusParameters() {
}

void ModbusParameters::setModbusPtr(Supla::Modbus::Configurator *modbus) {
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

  sender->send("<h3>Modbus Settings</h3>");

  // form-field BEGIN
  sender->send("<div class=\"form-field\">");
  sender->sendLabelFor(ModbusRoleTag, "Modbus mode");
  sender->send("<div>");
  sender->send("<select ");
  sender->sendNameAndId(ModbusRoleTag);
  sender->send(">");
  sender->sendSelectItem(
      0, "Disabled", config.role == Supla::Modbus::Role::NotSet);
  if (properties.protocol.master) {
    sender->sendSelectItem(
        1, "Master (client)", config.role == Supla::Modbus::Role::Master);
  }
  if (properties.protocol.slave) {
    sender->sendSelectItem(
        2, "Slave (server)", config.role == Supla::Modbus::Role::Slave);
  }
  sender->send("</select>");
  sender->send("</div>");
  sender->send("</div>");
  // form-field END

  if (properties.protocol.slave) {
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(ModbusAddressTag, "Modbus address (only for Slave)");
    sender->send("<div>");
    sender->send("<input type=\"number\" step=\"1\" min=\"1\" max=\"247\" ");
    sender->sendNameAndId(ModbusAddressTag);
    sender->send(" value=\"");
    sender->send(config.modbusAddress);
    sender->send("\">");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }

  // serial mode
  if (properties.protocol.rtu || properties.protocol.ascii) {
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(ModbusSerialModeTag, "Modbus serial mode");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(ModbusSerialModeTag);
    sender->send(">");
    sender->sendSelectItem(
        0,
        "Disabled",
        config.serial.mode == Supla::Modbus::ModeSerial::Disabled);
    if (properties.protocol.rtu) {
      sender->sendSelectItem(
          1, "RTU", config.serial.mode == Supla::Modbus::ModeSerial::Rtu);
    }
    if (properties.protocol.ascii) {
      sender->sendSelectItem(
          2, "ASCII", config.serial.mode == Supla::Modbus::ModeSerial::Ascii);
    }
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END

    // baudrate
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(ModbusBaudrateTag, "Baudrate");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(ModbusBaudrateTag);
    sender->send(">");
    if (properties.baudrate.b4800) {
      sender->sendSelectItem(
          4800, "4800", config.serial.baudrate == 4800);
    }
    if (properties.baudrate.b9600) {
      sender->sendSelectItem(
          9600, "9600", config.serial.baudrate == 9600);
    }
    if (properties.baudrate.b19200) {
      sender->sendSelectItem(
          19200, "19200", config.serial.baudrate == 19200);
    }
    if (properties.baudrate.b38400) {
      sender->sendSelectItem(
          38400, "38400", config.serial.baudrate == 38400);
    }
    if (properties.baudrate.b57600) {
      sender->sendSelectItem(
          57600, "57600", config.serial.baudrate == 57600);
    }
    if (properties.baudrate.b115200) {
      sender->sendSelectItem(
          115200, "115200", config.serial.baudrate == 115200);
    }
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END

    // stopbits
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(ModbusStopBitsTag, "Stop bits");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(ModbusStopBitsTag);
    sender->send(">");
    if (properties.stopBits.one) {
      sender->sendSelectItem(
          0, "1", config.serial.stopBits == Supla::Modbus::SerialStopBits::One);
    }
    if (properties.stopBits.oneAndHalf) {
      sender->sendSelectItem(
          1,
          "1.5",
          config.serial.stopBits == Supla::Modbus::SerialStopBits::OneAndHalf);
    }
    if (properties.stopBits.two) {
      sender->sendSelectItem(
          2, "2", config.serial.stopBits == Supla::Modbus::SerialStopBits::Two);
    }
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
  }

  // network mode
  if (properties.protocol.tcp || properties.protocol.udp) {
    // form-field BEGIN
    sender->send("<div class=\"form-field\">");
    sender->sendLabelFor(ModbusNetworkModeTag, "Modbus network mode");
    sender->send("<div>");
    sender->send("<select ");
    sender->sendNameAndId(ModbusNetworkModeTag);
    sender->send(">");
    sender->sendSelectItem(
        0,
        "Disabled",
        config.network.mode == Supla::Modbus::ModeNetwork::Disabled);
    if (properties.protocol.tcp) {
      sender->sendSelectItem(
          1, "TCP", config.network.mode == Supla::Modbus::ModeNetwork::Tcp);
    }
    if (properties.protocol.udp) {
      sender->sendSelectItem(
          2, "UDP", config.network.mode == Supla::Modbus::ModeNetwork::Udp);
    }
    sender->send("</select>");
    sender->send("</div>");
    sender->send("</div>");
    // form-field END
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

