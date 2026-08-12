// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
