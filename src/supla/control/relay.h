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

/* Relay class
 * This class is used to control any type of relay that can be controlled
 * by setting LOW or HIGH output on selected GPIO.
 */

#ifndef SRC_SUPLA_CONTROL_RELAY_H_
#define SRC_SUPLA_CONTROL_RELAY_H_

#include <stdint.h>

#include "../action_handler.h"
#include "../channel_element.h"

#define STATE_ON_INIT_RESTORED_OFF -3
#define STATE_ON_INIT_RESTORED_ON  -2
#define STATE_ON_INIT_RESTORE      -1
#define STATE_ON_INIT_OFF          0
#define STATE_ON_INIT_ON           1

#define RELAY_FLAGS_ON (1 << 0)
#define RELAY_FLAGS_STAIRCASE (1 << 1)
#define RELAY_FLAGS_IMPULSE_FUNCTION (1 << 2)  // i.e. gate, door, gateway
#define RELAY_FLAGS_OVERCURRENT (1 << 3)


namespace Supla {
class Io;

namespace Control {
class Button;

class Relay : public ChannelElement, public ActionHandler {
 public:
  explicit Relay(Supla::Io *io, int pin,
        bool highIsOn = true,
        _supla_int_t functions = (0xFF ^
                                  SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  explicit Relay(int pin,
        bool highIsOn = true,
        _supla_int_t functions = (0xFF ^
                                  SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  virtual ~Relay();

  virtual Relay &setDefaultStateOn();
  virtual Relay &setDefaultStateOff();
  virtual Relay &setDefaultStateRestore();
  virtual Relay &keepTurnOnDuration(bool keep = true);  // DEPREACATED

  virtual uint8_t pinOnValue();
  virtual uint8_t pinOffValue();
  virtual void turnOn(_supla_int_t duration = 0);
  virtual void turnOff(_supla_int_t duration = 0);
  virtual bool isOn();
  virtual void toggle(_supla_int_t duration = 0);

  void attach(Supla::Control::Button *);

  void handleAction(int event, int action) override;

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void onInit() override;
  void onLoadState() override;
  void onSaveState() override;
  void iterateAlways() override;
  bool iterateConnected() override;
  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                              bool local = false) override;

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

  unsigned _supla_int_t getStoredTurnOnDurationMs();

  bool isStaircaseFunction() const;
  bool isImpulseFunction() const;
  void disableCountdownTimerFunction();
  void enableCountdownTimerFunction();
  bool isCountdownTimerFunctionEnabled() const;
  void setMinimumAllowedDurationMs(uint32_t durationMs);
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

  static void setRelayStorageSaveDelay(int delayMs);

  bool isDefaultRelatedMeterChannelSet() const;
  uint32_t getCurrentValueFromMeter() const;
  void setDefaultRelatedMeterChannelNo(int channelNo);
  void setTurnOffWhenEmptyAggregator(bool turnOff);

  /**
   * Set overcurrent threshold
   *
   * @param value Value in 0.01 A. Value of 0 means disabled
   */
  void setOvercurrentThreshold(uint32_t value);

  /**
   * Get overcurrent threshold
   *
   * @return Value in 0.01 A
   */
  uint32_t getOvercurrentThreshold() const { return overcurrentThreshold; }

  /**
   * Set overcurrent max level allowed
   *
   * @param value Value in 0.01 A. Value of 0 means disabled
   */
  void setOvercurrentMaxAllowed(uint32_t value);

  /**
   * Get overcurrent max level allowed
   *
   * @return Value in 0.01 A
   */
  uint32_t getOvercurrentMaxAllowed() const { return overcurrentMaxAllowed; }

 protected:
  struct ButtonListElement {
    Supla::Control::Button *button = nullptr;
    ButtonListElement *next = nullptr;
  };

  void saveConfig() const;
  void setChannelFunction(_supla_int_t newFunction);
  void updateTimerValue();
  void updateRelayHvacAggregator();
  int32_t channelFunction = 0;
  uint32_t durationMs = 0;
  uint32_t storedTurnOnDurationMs = 0;
  uint32_t durationTimestamp = 0;

  uint32_t overcurrentThreshold = 0;   // 0.01 A
  uint32_t overcurrentMaxAllowed = 0;  // 0.01 A
  uint32_t overcurrentActiveTimestamp = 0;
  uint32_t overcurrentCheckTimestamp = 0;

  uint32_t timerUpdateTimestamp = 0;

  Supla::Io *io = nullptr;
  ButtonListElement *buttonList = nullptr;

  uint16_t minimumAllowedDurationMs = 0;
  int16_t pin = -1;

  bool highIsOn = true;
  bool keepTurnOnDurationMs = false;
  bool lastStateOnTimerUpdate = false;
  bool turnOffWhenEmptyAggregator = true;

  int8_t stateOnInit = STATE_ON_INIT_OFF;

  int16_t defaultRelatedMeterChannelNo = -1;

  static int16_t relayStorageSaveDelay;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_H_
