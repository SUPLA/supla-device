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
