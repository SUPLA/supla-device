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

#ifndef SRC_SUPLA_NETWORK_HTML_ROLLER_SHUTTER_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_ROLLER_SHUTTER_PARAMETERS_H_

#include <stdint.h>

#include <supla-common/proto.h>
#include <supla/network/html_element.h>

namespace Supla {

namespace Control {
class RollerShutter;
}

namespace Html {

class RollerShutterParameters : public HtmlElement {
 public:
  explicit RollerShutterParameters(
      Supla::Control::RollerShutter* rollerShutter = nullptr);
  virtual ~RollerShutterParameters();
  void send(Supla::WebSender* sender) override;
  bool handleResponse(const char* key, const char* value) override;
  void onProcessingEnd() override;

  void setRsPtr(Supla::Control::RollerShutter *rs);
  void setShowChannelFunction(bool show);
  void setRenderContainer(bool render);
  void setShowOnlyForRollerFunction(bool showOnly);
  void setDynamicVisibilityFromChannelFunction(bool enabled);

 protected:
  Supla::Control::RollerShutter *rs = nullptr;
  bool showChannelFunction = true;
  bool renderContainer = true;
  bool showOnlyForRollerFunction = false;
  bool dynamicVisibilityFromChannelFunction = false;

  bool pendingFacadeBlindTiming = false;
  bool pendingFacadeBlindTimingInvalid = false;
  uint32_t pendingOpeningTimeMs = 0;
  uint32_t pendingClosingTimeMs = 0;
  uint32_t pendingTiltingTimeMs = 0;
  uint32_t pendingTiltControlType = SUPLA_TILT_CONTROL_TYPE_UNKNOWN;
};

};  // namespace Html
};  // namespace Supla



#endif  // SRC_SUPLA_NETWORK_HTML_ROLLER_SHUTTER_PARAMETERS_H_
