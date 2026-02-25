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
#include <supla/control/rgb_cct_base.h>

#define RGB_CCT_MAX 5

namespace Supla {
namespace Control {
class RGBWLedsEspIdf : public RGBCCTBase {
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
  RGBWLedsEspIdf(Supla::Control::RGBWLedsEspIdf *parent,
                 int ledcTimerId,
                 int redPin,
                 int greenPin,
                 int bluePin,
                 int w1BrightnessPin,
                 int w2BrightnessPin);

  void setRGBCCTValueOnDevice(uint32_t red,
                              uint32_t green,
                              uint32_t blue,
                              uint32_t colorBrightness,
                              uint32_t w1Brightness,
                              uint32_t w2Brightness) override;

  void onInit() override;

  void setOutputInvert(bool outputInvert) {
    this->outputInvert = outputInvert;
  }

 protected:
  /**
   * Obtains ledc channel from parent instance based on given GPIO
   * In parent-child relationship, only one instance of RGBWLedsEspIdf is
   * allowed to initialize LEDC, other instances rely on ledc_counter from
   * parent
   *
   * @param gpio
   *
   * @return ledc channel
   */
  ledc_channel_t getLedcChannel(int gpio) const;

  int gpios[RGB_CCT_MAX] = {-1, -1, -1, -1, -1};
  int ledcTimerId = 0;
  static int pwmChannelCounter;
  ledc_channel_t channels[RGB_CCT_MAX] = {};

  ledc_channel_t initChannel(int gpio);
  bool initialized = false;
  bool enabled = true;
  bool outputInvert = false;

  int tryCounter = 0;

  int32_t channelPrevValue[RGB_CCT_MAX] = {-1, -1, -1, -1, -1};
  RGBWLedsEspIdf *parentLedsEspIdf = nullptr;
};

};  // namespace Control
};  // namespace Supla

#endif  // EXTRAS_ESP_IDF_RGBW_PWM_RGBW_PWM_H_
