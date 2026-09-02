// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
