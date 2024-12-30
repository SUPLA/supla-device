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

#ifndef SRC_SUPLA_NETWORK_HTML_CONTAINER_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_CONTAINER_PARAMETERS_H_

#include <supla/network/html_element.h>
#include <supla/sensor/container.h>

namespace Supla {

namespace Html {

class ContainerParameters : public HtmlElement {
 public:
  explicit ContainerParameters(bool allowSensors = false,
                               Supla::Sensor::Container *contaier = nullptr);
  virtual ~ContainerParameters();

  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

  void setContainerPtr(Supla::Sensor::Container *container);

 protected:
  int parseValue(const char *value) const;
  void generateSensorKey(char *key, const char *prefix, int index);
  Supla::Sensor::Container *container = nullptr;
  Supla::Sensor::ContainerConfig config;
  bool muteSet = false;
  bool configChanged = false;
  bool allowSensors = false;
};

};  // namespace Html
};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_CONTAINER_PARAMETERS_H_
