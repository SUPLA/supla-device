// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgbw_base.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <supla/log_wrapper.h>
#include <supla/control/button.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>

#include "../storage/storage.h"
#include "../time.h"
#include "../tools.h"
#include "supla/actions.h"

namespace Supla {
namespace Control {

RGBWBase::RGBWBase() {
  channel.setType(SUPLA_CHANNELTYPE_DIMMERANDRGBLED);
  channel.setDefault(SUPLA_CHANNELFNC_DIMMERANDRGBLIGHTING);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RGBW_COMMANDS_SUPPORTED);
  // RGBWBase works in legacy mode where Dimmer/RGB/RGBW function was
  // assigned to dedicated channel type.
  // Use LightingPwmBase class if you want to have flexible control over
  // Dimmer/RGB/RGBW/CCT functions
  channel.setFuncList(0);
}

void RGBWBase::onSaveState() {
  /*
  uint8_t curRed;                   // 0 - 255
  uint8_t curGreen;                 // 0 - 255
  uint8_t curBlue;                  // 0 - 255
  uint8_t curColorBrightness;       // 0 - 100
  uint8_t curWhiteBrightness;            // 0 - 100
  uint8_t lastColorBrightness;      // 0 - 100
  uint8_t lastWhiteBrightness;           // 0 - 100
  */
  Supla::Storage::WriteState((unsigned char *)&requested.red,
                             sizeof(requested.red));
  Supla::Storage::WriteState((unsigned char *)&requested.green,
                             sizeof(requested.green));
  Supla::Storage::WriteState((unsigned char *)&requested.blue,
                             sizeof(requested.blue));
  Supla::Storage::WriteState((unsigned char *)&requested.colorBrightness,
                             sizeof(requested.colorBrightness));
  Supla::Storage::WriteState((unsigned char *)&requested.whiteBrightness,
                             sizeof(requested.whiteBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastNonZero.colorBrightness,
                             sizeof(lastNonZero.colorBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastNonZero.whiteBrightness,
                             sizeof(lastNonZero.whiteBrightness));
}

void RGBWBase::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&requested.red,
                            sizeof(requested.red));
  Supla::Storage::ReadState((unsigned char *)&requested.green,
                            sizeof(requested.green));
  Supla::Storage::ReadState((unsigned char *)&requested.blue,
                            sizeof(requested.blue));
  Supla::Storage::ReadState((unsigned char *)&requested.colorBrightness,
                            sizeof(requested.colorBrightness));
  Supla::Storage::ReadState((unsigned char *)&requested.whiteBrightness,
                            sizeof(requested.whiteBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastNonZero.colorBrightness,
                            sizeof(lastNonZero.colorBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastNonZero.whiteBrightness,
                            sizeof(lastNonZero.whiteBrightness));
  requested.whiteTemperature = 0;

  SUPLA_LOG_DEBUG(
      "RGBWBase[%d] loaded state: red=%d, green=%d, blue=%d, "
      "colorBrightness=%d, whiteBrightness=%d",
      getChannel()->getChannelNumber(),
      requested.red,
      requested.green,
      requested.blue,
      requested.colorBrightness,
      requested.whiteBrightness);
}

void RGBWBase::setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) {
  (void)(usedOutputs);
  setRGBWValueOnDevice(output[0], output[1], output[2], output[3]);
}

};  // namespace Control
};  // namespace Supla
