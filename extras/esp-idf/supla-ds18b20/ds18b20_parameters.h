// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_ESP_IDF_SUPLA_DS18B20_DS18B20_PARAMETERS_H_
#define EXTRAS_ESP_IDF_SUPLA_DS18B20_DS18B20_PARAMETERS_H_

#include <supla/network/html_element.h>

namespace Supla {

namespace Html {

class DS18B20Parameters : public HtmlElement {
 public:
  explicit DS18B20Parameters(int channel);
  virtual ~DS18B20Parameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;

 protected:
  int channel = -1;
};

};  // namespace Html
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_DS18B20_DS18B20_PARAMETERS_H_
