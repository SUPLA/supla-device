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
