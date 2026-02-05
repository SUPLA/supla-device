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

#ifndef EXTRAS_ESP_IDF_RGBW_PWM_DIMMER_PWM_H_
#define EXTRAS_ESP_IDF_RGBW_PWM_DIMMER_PWM_H_

#include "rgbw_pwm.h"

namespace Supla::Control {
class DimmerLedEspIdf : public RGBWLedsEspIdf {
 public:
  DimmerLedEspIdf(Supla::Control::RGBWLedsEspIdf *parent,
                  int ledcTimerId,
                  int gpio1,
                  int gpio2 = -1,
                  int gpio3 = -1,
                  int gpio4 = -1,
                  int gpio5 = -1);
};

}  // namespace Supla::Control

#endif  // EXTRAS_ESP_IDF_RGBW_PWM_DIMMER_PWM_H_

