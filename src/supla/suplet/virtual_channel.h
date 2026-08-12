// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_VIRTUAL_CHANNEL_H_
#define SRC_SUPLA_SUPLET_VIRTUAL_CHANNEL_H_

#include <supla/control/virtual_relay.h>
#include <supla/sensor/virtual_binary.h>
#include <supla/sensor/virtual_thermometer.h>

namespace Supla {
namespace Suplet {

class VirtualRelay : public Supla::Control::VirtualRelay {
 public:
  VirtualRelay(uint8_t subDeviceId,
               int channelNumber,
               _supla_int_t functions =
                   (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  bool isOwnerOfSubDeviceId(int subDeviceId) const override;

 private:
  uint8_t subDeviceId = 0;
};

class VirtualBinarySensor : public Supla::Sensor::VirtualBinary {
 public:
  VirtualBinarySensor(uint8_t subDeviceId,
                      int channelNumber,
                      bool keepStateInStorage = false);

  bool isOwnerOfSubDeviceId(int subDeviceId) const override;

 private:
  uint8_t subDeviceId = 0;
};

class VirtualThermometer : public Supla::Sensor::VirtualThermometer {
 public:
  VirtualThermometer(uint8_t subDeviceId, int channelNumber);

  bool isOwnerOfSubDeviceId(int subDeviceId) const override;

 private:
  uint8_t subDeviceId = 0;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_VIRTUAL_CHANNEL_H_
