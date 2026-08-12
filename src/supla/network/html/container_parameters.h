// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
