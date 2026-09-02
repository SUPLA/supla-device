// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_HVAC_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_HVAC_PARAMETERS_H_

#include <supla/network/html_element.h>
#include <supla/control/hvac_base.h>

namespace Supla {

namespace Html {

class HvacParameters : public HtmlElement {
 public:
  explicit HvacParameters(Supla::Control::HvacBase *hvac = nullptr);
  virtual ~HvacParameters();

  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

  void setHvacPtr(Supla::Control::HvacBase *hvac);

 protected:
  Supla::Control::HvacBase *hvac = nullptr;
  TSD_SuplaChannelNewValue *newValue = nullptr;
  THVACValue *hvacValue = nullptr;
  TSD_ChannelConfig *config = nullptr;
  TChannelConfig_HVAC *hvacConfig = nullptr;
};

};  // namespace Html
};  // namespace Supla


#endif  // SRC_SUPLA_NETWORK_HTML_HVAC_PARAMETERS_H_
