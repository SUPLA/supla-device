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

#ifndef SRC_SUPLA_SENSOR_OCR_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_OCR_IMPULSE_COUNTER_H_

#include "virtual_impulse_counter.h"

namespace Supla {

namespace Sensor {
class OcrImpulseCounter : public VirtualImpulseCounter {
 public:
  OcrImpulseCounter();

 protected:
  bool hasOcrConfig() override;
  void clearOcrConfig() override;
  bool isOcrConfigMissing() override;
  uint8_t applyChannelConfig(TSD_ChannelConfig *result, bool local) override;
  void fillChannelConfig(void *channelConfig, int *size) override;
  void fillChannelOcrConfig(void *channelConfig, int *size) override;

  TChannelConfig_OCR ocrConfig;
  bool ocrConfigReceived = false;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_OCR_IMPULSE_COUNTER_H_
