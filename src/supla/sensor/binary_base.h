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

#ifndef SRC_SUPLA_SENSOR_BINARY_BASE_H_
#define SRC_SUPLA_SENSOR_BINARY_BASE_H_

#include <supla/channels/binary_sensor_channel.h>
#include <supla/element_with_channel_actions.h>

class SuplaDeviceClass;

namespace Supla {

class Io;

namespace Sensor {
class BinaryBase : public ElementWithChannelActions {
 public:
  BinaryBase();
  virtual ~BinaryBase();
  virtual bool getValue() = 0;
  void iterateAlways() override;
  Channel *getChannel() override;
  const Channel *getChannel() const override;
  void onLoadConfig(SuplaDeviceClass *) override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *config,
                              bool local = false) override;
  void handleChannelConfigFinished() override;

  void setServerInvertLogic(bool invertLogic);
  void setReadIntervalMs(uint32_t intervalMs);
  void purgeConfig() override;

 protected:
  uint32_t lastReadTime = 0;
  uint32_t readIntervalMs = 100;
  BinarySensorChannel channel;
  bool configFinishedReceived = false;
  bool defaultConfigReceived = false;
};

}  // namespace Sensor
}  // namespace Supla


#endif  // SRC_SUPLA_SENSOR_BINARY_BASE_H_
