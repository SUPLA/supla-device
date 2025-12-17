/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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
  // Use RGBCCTBase class if you want to have flexible control over
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
  Supla::Storage::WriteState((unsigned char *)&curRed, sizeof(curRed));
  Supla::Storage::WriteState((unsigned char *)&curGreen, sizeof(curGreen));
  Supla::Storage::WriteState((unsigned char *)&curBlue, sizeof(curBlue));
  Supla::Storage::WriteState((unsigned char *)&curColorBrightness,
                             sizeof(curColorBrightness));
  Supla::Storage::WriteState((unsigned char *)&curWhiteBrightness,
                             sizeof(curWhiteBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastColorBrightness,
                             sizeof(lastColorBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastWhiteBrightness,
                             sizeof(lastWhiteBrightness));
}

void RGBWBase::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&curRed, sizeof(curRed));
  Supla::Storage::ReadState((unsigned char *)&curGreen, sizeof(curGreen));
  Supla::Storage::ReadState((unsigned char *)&curBlue, sizeof(curBlue));
  Supla::Storage::ReadState((unsigned char *)&curColorBrightness,
                            sizeof(curColorBrightness));
  Supla::Storage::ReadState((unsigned char *)&curWhiteBrightness,
                            sizeof(curWhiteBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastColorBrightness,
                            sizeof(lastColorBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastWhiteBrightness,
                            sizeof(lastWhiteBrightness));
  curWhiteTemperature = 0;

  SUPLA_LOG_DEBUG(
      "RGBWBase[%d] loaded state: red=%d, green=%d, blue=%d, "
      "colorBrightness=%d, whiteBrightness=%d",
      getChannel()->getChannelNumber(),
      curRed,
      curGreen,
      curBlue,
      curColorBrightness,
      curWhiteBrightness);
}

void RGBWBase::setRGBCCTValueOnDevice(uint32_t red,
                                      uint32_t green,
                                      uint32_t blue,
                                      uint32_t colorBrightness,
                                      uint32_t white1Brightness,
                                      uint32_t white2Brightness) {
  (void)(white2Brightness);
  setRGBWValueOnDevice(red, green, blue, colorBrightness, white1Brightness);
}

};  // namespace Control
};  // namespace Supla
