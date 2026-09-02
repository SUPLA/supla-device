// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
                            int resultBufferSize,
                            const char *cropSettings) override;
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
