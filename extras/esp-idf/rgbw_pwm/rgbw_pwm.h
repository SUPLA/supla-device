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

#ifndef EXTRAS_ESP_IDF_RGBW_PWM_RGBW_PWM_H_
#define EXTRAS_ESP_IDF_RGBW_PWM_RGBW_PWM_H_

#include <driver/ledc.h>
#include <supla/control/rgbw_pwm_base.h>

namespace Supla {
namespace Control {
class RGBWLedsEspIdf : public RGBWPwmBase {
 public:
  /**
   * Constructor
   *
   * @param parent parent RGBW object - set to null, for first instance, then
   * remaining instances should be passed one after another
   * @param ledcTimerId
   * @param redPin aka GPIO[0] -> "red" is for backward compatibility, actual
   * meaning is just GPIO[0], which may be used also for dimmer, etc. -
   * depending on configured function
   * @param greenPin as above -> GPIO[1]
   * @param bluePin as above -> GPIO[2]
   * @param w1BrightnessPin as above -> GPIO[3]
   * @param w2BrightnessPin as above -> GPIO[4]
   */
  RGBWLedsEspIdf(Supla::Control::RGBWPwmBase *parent,
                 int ledcTimerId,
                 int out1Gpio,
                 int out2Gpio,
                 int out3Gpio,
                 int out4Gpio,
                 int out5Gpio);

 protected:
  void initializePwmTimer() override;
  int initializePwmChannel(int gpio) override;
  void writePwmValue(int channel, uint32_t value) override;
  void stopPwmChannel(int channel) override;

  int ledcTimerId = 0;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_RGBW_PWM_RGBW_PWM_H_
