// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "rgb_base.h"
#include "../storage/storage.h"

Supla::Control::RGBBase::RGBBase() {
  channel.setType(SUPLA_CHANNELTYPE_RGBLEDCONTROLLER);
  channel.setDefaultFunction(SUPLA_CHANNELFNC_RGBLIGHTING);
}

void Supla::Control::RGBBase::setRGBCCT(int red,
                                        int green,
                                        int blue,
                                        int colorBrightness,
                                        int brightness,
                                        int whileTemperature,
                                        bool toggle,
                                        bool instant) {
  (void)(brightness);
  (void)(whileTemperature);
  Supla::Control::RGBWBase::setRGBCCT(
      red, green, blue, colorBrightness, 0, 0, toggle, instant);
}

void Supla::Control::RGBBase::onSaveState() {
  /*
  uint8_t curRed;                   // 0 - 255
  uint8_t curGreen;                 // 0 - 255
  uint8_t curBlue;                  // 0 - 255
  uint8_t curColorBrightness;       // 0 - 100
  uint8_t lastColorBrightness;      // 0 - 100
  */
  Supla::Storage::WriteState((unsigned char *)&requested.red,
                             sizeof(requested.red));
  Supla::Storage::WriteState((unsigned char *)&requested.green,
                             sizeof(requested.green));
  Supla::Storage::WriteState((unsigned char *)&requested.blue,
                             sizeof(requested.blue));
  Supla::Storage::WriteState((unsigned char *)&requested.colorBrightness,
                             sizeof(requested.colorBrightness));
  Supla::Storage::WriteState((unsigned char *)&lastNonZero.colorBrightness,
                             sizeof(lastNonZero.colorBrightness));
}

void Supla::Control::RGBBase::onLoadState() {
  Supla::Storage::ReadState((unsigned char *)&requested.red,
                            sizeof(requested.red));
  Supla::Storage::ReadState((unsigned char *)&requested.green,
                            sizeof(requested.green));
  Supla::Storage::ReadState((unsigned char *)&requested.blue,
                            sizeof(requested.blue));
  Supla::Storage::ReadState((unsigned char *)&requested.colorBrightness,
                            sizeof(requested.colorBrightness));
  Supla::Storage::ReadState((unsigned char *)&lastNonZero.colorBrightness,
                            sizeof(lastNonZero.colorBrightness));
}
