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

#include "modbus_configurator_esp_idf.h"

#include <esp_idf_wifi.h>
#include <supla/log_wrapper.h>
#include <supla/modbus/modbus_client_handler.h>
#include <supla/network/network.h>

#include <esp_modbus_slave.h>
#include "mbcontroller.h"

using Supla::Modbus::ConfiguratorEspIdf;


namespace {
mb_err_enum_t SuplaResultToEspModbusCode(Supla::Modbus::Result result) {
  switch (result) {
    case Supla::Modbus::Result::OK:
      return MB_ENOERR;
    case Supla::Modbus::Result::INVALID_REGISTER_ADDRESS:
      return MB_ENOREG;
    case Supla::Modbus::Result::INVALID_STATE:
      return MB_EILLSTATE;
  }
  return MB_EILLSTATE;
}
}  // namespace

/**
 * Override modbus slave callback (weak linkage)
 *
 * @param inst
 * @param reg_buffer
 * @param address
 * @param n_regs
 * @param mode
 *
 * @return MB error code
 */
mb_err_enum_t mbc_reg_holding_slave_cb(mb_base_t *inst,
                                       uint8_t *reg_buffer,
                                       uint16_t address,
                                       uint16_t n_regs,
                                       mb_reg_mode_enum_t mode) {
  if (inst == nullptr) {
    SUPLA_LOG_ERROR("MB reg_holding_slave: inst is null");
    return MB_EILLSTATE;
  }
  if (!Supla::ModbusClientHandler::IsHoldingSupported()) {
    return MB_EILLFUNC;
  }

  // esp-modbus is incrementing address by 1, so it is 1..n. We use 0..n
  address--;

  return SuplaResultToEspModbusCode(
      Supla::ModbusClientHandler::HoldingProcessRequest(
          address,
          n_regs,
          reg_buffer,
          mode == MB_REG_WRITE ? Supla::Modbus::Access::WRITE
                               : Supla::Modbus::Access::READ));
}

mb_err_enum_t mbc_reg_input_slave_cb(mb_base_t *inst,
                                     uint8_t *reg_buffer,
                                     uint16_t address,
                                     uint16_t n_regs) {
  if (inst == nullptr) {
    SUPLA_LOG_ERROR("MB reg_input_slave: inst is null");
    return MB_EILLSTATE;
  }
  if (!Supla::ModbusClientHandler::IsInputSupported()) {
    return MB_EILLFUNC;
  }

  // esp-modbus is incrementing address by 1, so it is 1..n. We use 0..n
  address--;

  return SuplaResultToEspModbusCode(
      Supla::ModbusClientHandler::InputProcessRequest(
          address, n_regs, reg_buffer));
}

mb_err_enum_t mbc_reg_coils_slave_cb(mb_base_t *inst,
                                     uint8_t *reg_buffer,
                                     uint16_t address,
                                     uint16_t n_coils,
                                     mb_reg_mode_enum_t mode) {
  if (inst == nullptr) {
    SUPLA_LOG_ERROR("MB reg_coils_slave: inst is null");
    return MB_EILLSTATE;
  }
  if (!Supla::ModbusClientHandler::IsCoilsSupported()) {
    return MB_EILLFUNC;
  }

  // esp-modbus is incrementing address by 1, so it is 1..n. We use 0..n
  address--;

  return SuplaResultToEspModbusCode(
      Supla::ModbusClientHandler::CoilsProcessRequest(
          address,
          n_coils,
          reg_buffer,
          mode == MB_REG_WRITE ? Supla::Modbus::Access::WRITE
                               : Supla::Modbus::Access::READ));
}

mb_err_enum_t mbc_reg_discrete_slave_cb(mb_base_t *inst,
                                        uint8_t *reg_buffer,
                                        uint16_t address,
                                        uint16_t n_discrete) {
  if (inst == nullptr) {
    SUPLA_LOG_ERROR("MB reg_discrete_slave: inst is null");
    return MB_EILLSTATE;
  }
  if (!Supla::ModbusClientHandler::IsDiscreteSupported()) {
    return MB_EILLFUNC;
  }

  // esp-modbus is incrementing address by 1, so it is 1..n. We use 0..n
  address--;

  return SuplaResultToEspModbusCode(
      Supla::ModbusClientHandler::DiscreteProcessRequest(
          address, n_discrete, reg_buffer));
}

// End of esp-idf specific part
// ****************************************************************************

ConfiguratorEspIdf::ConfiguratorEspIdf(int txGpio,
                                       int rxGpio,
                                       int txEnGpio,
                                       Supla::EspIdfWifi *wifi)
    : txGpio(txGpio), rxGpio(rxGpio), txEnGpio(txEnGpio), wifi(wifi) {
  config.role = Supla::Modbus::Role::Slave;
  config.serial.mode = Supla::Modbus::ModeSerial::Rtu;
}

void ConfiguratorEspIdf::iterateAlways() {
  if (configChanged) {
    if (mbcSlaveHandleSerial) {
      SUPLA_LOG_INFO("ConfiguratorEspIdf: reconfigure Modbus serial stack");
      ESP_ERROR_CHECK(mbc_slave_delete(mbcSlaveHandleSerial));
    }
    if (mbcSlaveHandleTcp) {
      SUPLA_LOG_INFO("ConfiguratorEspIdf: reconfigure Modbus TCP stack");
      ESP_ERROR_CHECK(mbc_slave_delete(mbcSlaveHandleTcp));
    }
    mbcSlaveHandleSerial = nullptr;
    mbcSlaveHandleTcp = nullptr;
    isModbusStackStarted = false;
    configChanged = false;
  }

  if (isSerialModeEnabled() && !mbcSlaveHandleSerial) {
    setupSerialModbus();
  }

  if (isNetworkModeEnabled() && !mbcSlaveHandleTcp) {
    if (!Supla::Network::IsReady()) {
      return;
    }
    setupNetworkModbus();
  }
}

bool ConfiguratorEspIdf::setupSerialModbus() {
  SUPLA_LOG_INFO(
      "********************* MODBUS SERIAL CONFIGURATION ********************");
  printConfig();
  mb_communication_info_t commConfig = {};
  commConfig.ser_opts.port = uartNum;
  commConfig.ser_opts.mode = isRtuMode() ? MB_RTU : MB_ASCII;
  commConfig.ser_opts.baudrate = config.serial.baudrate;
  commConfig.ser_opts.parity = MB_PARITY_NONE;
  commConfig.ser_opts.uid = config.modbusAddress;
  commConfig.ser_opts.data_bits = UART_DATA_8_BITS;
  if (config.serial.stopBits == SerialStopBits::One) {
    commConfig.ser_opts.stop_bits = UART_STOP_BITS_1;
  } else if (config.serial.stopBits == SerialStopBits::Two) {
    commConfig.ser_opts.stop_bits = UART_STOP_BITS_2;
  } else if (config.serial.stopBits == SerialStopBits::OneAndHalf) {
    commConfig.ser_opts.stop_bits = UART_STOP_BITS_1_5;
  }

  ESP_ERROR_CHECK(mbc_slave_create_serial(&commConfig, &mbcSlaveHandleSerial));

  ESP_ERROR_CHECK(uart_set_pin(uartNum, txGpio, rxGpio, txEnGpio, -1));
  ESP_ERROR_CHECK(uart_set_mode(uartNum, UART_MODE_RS485_HALF_DUPLEX));

  // Starts of modbus controller and stack
  esp_err_t err = mbc_slave_start(mbcSlaveHandleSerial);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Modbus slave start error: %d", err);
  }

  SUPLA_LOG_INFO(
      "*********************  MODBUS SERIAL CONFIGURATION DONE  "
      "********************");

  return err == ESP_OK;
}

bool ConfiguratorEspIdf::setupNetworkModbus() {
  SUPLA_LOG_INFO(
      "********************* MODBUS TCP CONFIGURATION ********************");
  printConfig();
  mb_communication_info_t tcpSlaveConfig = {};
  tcpSlaveConfig.tcp_opts.port = 502;
  tcpSlaveConfig.tcp_opts.mode = MB_TCP;
  tcpSlaveConfig.tcp_opts.addr_type = MB_IPV4;
  tcpSlaveConfig.tcp_opts.ip_addr_table = NULL;
  tcpSlaveConfig.tcp_opts.ip_netif_ptr = wifi->getStaNetIf();
  tcpSlaveConfig.tcp_opts.uid = 1;

  SUPLA_LOG_INFO("slave create tcp");
  ESP_ERROR_CHECK(mbc_slave_create_tcp(&tcpSlaveConfig, &mbcSlaveHandleTcp));

  // Setup communication parameters and start stack

  // Starts of modbus controller and stack
  SUPLA_LOG_INFO("slave start tcp");
  esp_err_t err = mbc_slave_start(mbcSlaveHandleTcp);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Modbus slave start error: %d", err);
  } else {
    SUPLA_LOG_INFO("Modbus slave started");
  }

  SUPLA_LOG_INFO(
      "*********************  MODBUS TCP CONFIGURATION DONE  "
      "********************");

  return err == ESP_OK;
}
