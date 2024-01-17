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

#ifndef SRC_SUPLA_SENSOR_GENERAL_PURPOSE_METER_H_
#define SRC_SUPLA_SENSOR_GENERAL_PURPOSE_METER_H_

#include "general_purpose_channel_base.h"

namespace Supla {
namespace Sensor {
class GeneralPurposeMeter : public GeneralPurposeChannelBase {
 public:
#pragma pack(push, 1)
  struct GPMMeterSpecificConfig {
    uint8_t counterType = 0;
    uint8_t includeValueAddedInHistory = 0;
    uint8_t fillMissingData = 0;
  };
#pragma pack(pop)

  explicit GeneralPurposeMeter(MeasurementDriver *driver = nullptr);

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  uint8_t applyChannelConfig(TSD_ChannelConfig *result) override;
  void fillChannelConfig(void *channelConfig, int *size) override;

  uint8_t getCounterType() const;
  uint8_t getIncludeValueAddedInHistory() const;
  uint8_t getFillMissingData() const;

  void setCounterType(uint8_t counterType, bool local = true);
  void setIncludeValueAddedInHistory(uint8_t includeValueAddedInHistory,
                                     bool local = true);
  void setFillMissingData(uint8_t fillMissingData, bool local = true);

 protected:
  GPMMeterSpecificConfig meterSpecificConfig = {};
};

};  // namespace Sensor
};  // namespace Supla
#endif  // SRC_SUPLA_SENSOR_GENERAL_PURPOSE_METER_H_
