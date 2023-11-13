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

#include "../storage/storage.h"
#include "dimmer_base.h"

Supla::Control::DimmerBase::DimmerBase() {
  channel.setType(SUPLA_CHANNELTYPE_DIMMER);
  channel.setDefault(SUPLA_CHANNELFNC_DIMMER);
}

void Supla::Control::DimmerBase::setRGBW(int red,
                                         int green,
                                         int blue,
                                         int colorBrightness,
                                         int brightness,
                                         bool toggle,
                                         bool instant) {
  (void)(red);
  (void)(green);
  (void)(blue);
  (void)(colorBrightness);
  Supla::Control::RGBWBase::setRGBW(0, 0, 0, 0, brightness, toggle, instant);
}

void Supla::Control::DimmerBase::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&curBrightness,
                            sizeof(curBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastBrightness,
                            sizeof(lastBrightness));
}

void Supla::Control::DimmerBase::onSaveState() {
  Supla::Storage::WriteState((unsigned char *)&curBrightness,
                             sizeof(curBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastBrightness,
                             sizeof(lastBrightness));
}

void Supla::Control::DimmerBase::iterateDimmerRGBW(int rgbStep, int wStep) {
  (void)(rgbStep);
  Supla::Control::RGBWBase::iterateDimmerRGBW(0, wStep);
}
