// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_DIMMER_BASE_H_
#define SRC_SUPLA_CONTROL_DIMMER_BASE_H_

#include "rgbw_base.h"

namespace Supla {
namespace Control {
class DimmerBase : public RGBWBase {
 public:
  DimmerBase();

  void setRGBCCT(int red,
                 int green,
                 int blue,
                 int colorBrightness,
                 int brightness,
                 int whiteTemperature,
                 bool toggle = false,
                 bool instant = false) override;

  void onLoadState() override;
  void onSaveState() override;

  void setRGBCCTValueOnDevice(uint32_t output[5], int usedOutputs) override;

 protected:
  void iterateDimmerRGBW(int rgbStep, int wStep) override;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_DIMMER_BASE_H_
