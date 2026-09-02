// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_BINARY_SENSOR_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_BINARY_SENSOR_PARAMETERS_H_

#include <supla/network/html_element.h>

namespace Supla {
class WebSender;

namespace Sensor {
class BinaryBase;
}  // namespace Sensor

namespace Html {

class BinarySensorParameters : public HtmlElement {
 public:
  explicit BinarySensorParameters(Supla::Sensor::BinaryBase *binary);
  virtual ~BinarySensorParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

 private:
  Supla::Sensor::BinaryBase *binary = nullptr;
  bool checkboxFound = false;
  bool configChanged = false;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_BINARY_SENSOR_PARAMETERS_H_

