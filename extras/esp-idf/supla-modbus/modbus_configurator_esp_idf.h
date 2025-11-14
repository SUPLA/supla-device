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

#ifndef EXTRAS_ESP_IDF_SUPLA_MODBUS_MODBUS_CONFIGURATOR_ESP_IDF_H_
#define EXTRAS_ESP_IDF_SUPLA_MODBUS_MODBUS_CONFIGURATOR_ESP_IDF_H_

#include <supla/modbus/modbus_configurator.h>

#include <driver/uart.h>

namespace Supla {

class EspIdfWifi;

namespace Modbus {

class ConfiguratorEspIdf : public Supla::Modbus::Configurator {
 public:
  ConfiguratorEspIdf(int txGpio,
                     int rxGpio,
                     int txEnGpio,
                     Supla::EspIdfWifi *wifi);

  void iterateAlways() override;

 private:
  bool setupSerialModbus();
  bool setupNetworkModbus();
  bool isModbusStackStarted = false;
  int txGpio = -1;
  int rxGpio = -1;
  int txEnGpio = -1;
  const uart_port_t uartNum = UART_NUM_2;
  void *mbcSlaveHandleSerial = nullptr;
  void *mbcSlaveHandleTcp = nullptr;
  Supla::EspIdfWifi *wifi = nullptr;
};

}  // namespace Modbus
}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_MODBUS_MODBUS_CONFIGURATOR_ESP_IDF_H_
