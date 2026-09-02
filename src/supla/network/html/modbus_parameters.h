// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_MODBUS_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_MODBUS_PARAMETERS_H_

#include <supla/network/html_element.h>
#include <supla/modbus/modbus_configurator.h>

namespace Supla {
namespace Modbus {
class Configurator;
}  // namespace Modbus

namespace Html {
class ModbusParameters : public HtmlElement {
 public:
  explicit ModbusParameters(Supla::Modbus::Configurator *modbus = nullptr);
  virtual ~ModbusParameters();

  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

  void setModbusPtr(Supla::Modbus::Configurator *modbus);

 protected:
  Supla::Modbus::Configurator *modbus = nullptr;
  Supla::Modbus::Config config;
  bool configChanged = false;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_MODBUS_PARAMETERS_H_
