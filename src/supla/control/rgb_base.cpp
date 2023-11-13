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

#include "rgb_base.h"
#include "../storage/storage.h"

Supla::Control::RGBBase::RGBBase() {
  channel.setType(SUPLA_CHANNELTYPE_RGBLEDCONTROLLER);
  channel.setDefault(SUPLA_CHANNELFNC_RGBLIGHTING);
}

void Supla::Control::RGBBase::setRGBW(int red,
                                      int green,
                                      int blue,
                                      int colorBrightness,
                                      int brightness,
                                      bool toggle,
                                      bool instant) {
  (void)(brightness);
  Supla::Control::RGBWBase::setRGBW(
      red, green, blue, colorBrightness, 0, toggle, instant);
}

void Supla::Control::RGBBase::onSaveState() {
  /*
  uint8_t curRed;                   // 0 - 255
  uint8_t curGreen;                 // 0 - 255
  uint8_t curBlue;                  // 0 - 255
  uint8_t curColorBrightness;       // 0 - 100
  uint8_t lastColorBrightness;      // 0 - 100
  */
  Supla::Storage::WriteState((unsigned char *)&curRed, sizeof(curRed));
  Supla::Storage::WriteState((unsigned char *)&curGreen, sizeof(curGreen));
  Supla::Storage::WriteState((unsigned char *)&curBlue, sizeof(curBlue));
  Supla::Storage::WriteState((unsigned char *)&curColorBrightness,
                             sizeof(curColorBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastColorBrightness,
                             sizeof(lastColorBrightness));
}

void Supla::Control::RGBBase::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&curRed, sizeof(curRed));
  Supla::Storage::ReadState((unsigned char *)&curGreen, sizeof(curGreen));
  Supla::Storage::ReadState((unsigned char *)&curBlue, sizeof(curBlue));
  Supla::Storage::ReadState((unsigned char *)&curColorBrightness,
                             sizeof(curColorBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastColorBrightness,
                             sizeof(lastColorBrightness));
}

