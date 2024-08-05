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

#ifndef EXTRAS_ESP_IDF_SUPLA_OCR_IC_SUPLA_OCR_IC_H_
#define EXTRAS_ESP_IDF_SUPLA_OCR_IC_SUPLA_OCR_IC_H_

#include <supla/sensor/ocr_impulse_counter.h>
#include <esp_camera.h>

namespace Supla {

class OcrIc : public Supla::Sensor::OcrImpulseCounter {
 public:
  explicit OcrIc(int ledGpio = -1);
  void onInit() override;

 protected:
  bool takePhoto() override;
  void releasePhoto() override;
  bool sendPhotoToOcrServer(const char *url,
                            const char *authkey,
                            char *resultBuffer,
                            int resultBufferSize) override;
  bool getStatusFromOcrServer(const char *url,
                              const char *authkey,
                              char *buf,
                              int size) override;
  void setLedState(int state) override;

  camera_fb_t* fb = nullptr;
  int ledGpio = -1;
};

}  // namespace Supla

#endif  // EXTRAS_ESP_IDF_SUPLA_OCR_IC_SUPLA_OCR_IC_H_
