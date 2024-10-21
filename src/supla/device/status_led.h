/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_DEVICE_STATUS_LED_H_
#define SRC_SUPLA_DEVICE_STATUS_LED_H_

#include <supla/control/blinking_led.h>

namespace Supla {

enum LedMode : uint8_t {
  LED_ON_WHEN_CONNECTED /* default */,
  LED_OFF_WHEN_CONNECTED,
  LED_ALWAYS_OFF,
  LED_IN_CONFIG_MODE_ONLY
};

// States SERVER_CONNECTING, REGISTERED_AND_READY, PACZKOW_WE_HAVE_A_PROBLEM
// may be different in multiprotocol scenario. We consider error as highest
// priority LED status, then SERVER_CONNECTING, and lowest priority has
// REGISTERED_AND_READY state. So for all states where Supla protocol has
// SERVER_CONNECTING and REGISTERED_AND_READY we check also other protocol
// layers state if they doesn't have any higher priority state.
enum LedSequence : uint8_t {
  NETWORK_CONNECTING /* initial state 2000/2000 ms */,
  SERVER_CONNECTING /* flashing 500/500 ms */,
  REGISTERED_AND_READY /* stable ON or OFF depending on config */,
  CONFIG_MODE /* quick flashing 100/100 ms */,
  SW_DOWNLOAD /* very fast flashing 20/20 ms */,
  PACZKOW_WE_HAVE_A_PROBLEM /* some problem 300/100 ms */,
  TESTING_PROCEDURE, /* used to indicate almost finished test 50/50 ms */
  CUSTOM_SEQUENCE /* values set manually, state changes ignored */
};

namespace Device {


class StatusLed : public Supla::Control::BlinkingLed {
 public:
  explicit StatusLed(Supla::Io *io, uint8_t outPin, bool invert = false);
  explicit StatusLed(uint8_t outPin, bool invert = false);

  void onLoadConfig(SuplaDeviceClass *) override;
  void iterateAlways() override;
  void onDeviceConfigChange(uint64_t fieldBit) override;

  // Enables custom LED sequence based on given durations.
  // Automatic sequence change will be disabled.
  void setCustomSequence(uint32_t onDurationMs,
                         uint32_t offDurationMs,
                         uint32_t pauseDurrationMs = 0,
                         uint8_t onLimit = 0,
                         uint8_t repeatLimit = 0) override;

  // Restores automatic LED sequence change based on device state.
  // It is enabled by default, so if it wasn't disabled by calling
  // setCustomSequence, then there is no need to call it.
  void setAutoSequence();

  // Sets status LED mode
  void setMode(LedMode newMode);
  LedMode getMode() const;
  void storeModeToConfig();
  void setDefaultMode(enum LedMode newMode);
  void setUseDeviceConfig(bool value);
  void identify();

 protected:
  LedSequence currentSequence = NETWORK_CONNECTING;
  LedMode ledMode = LED_ON_WHEN_CONNECTED;
  int8_t defaultMode = 0;
  bool useDeviceConfig = true;
};

}  // namespace Device
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_STATUS_LED_H_
