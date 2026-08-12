// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

/* Relay class
 * This class is used to control any type of relay that can be controlled
 * by setting LOW or HIGH output on selected GPIO.
 */

#ifndef SRC_SUPLA_CONTROL_RELAY_H_
#define SRC_SUPLA_CONTROL_RELAY_H_

#include <stdint.h>

#include "../action_handler.h"
#include "../channel_element.h"
#include "../io.h"

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
namespace Io {
struct IoPin;
}

namespace Control {
class Button;

class Relay : public ChannelElement, public ActionHandler {
 public:
  explicit Relay(Supla::Io::IoPin outputPin,
                 _supla_int_t functions =
                     (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  explicit Relay(Supla::Io::Base *io, int pin,
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
  virtual Relay &setPreloadStateOnSoftReset(bool enabled = true);
  virtual Relay &keepTurnOnDuration(bool keep = true);  // DEPREACATED

  [[deprecated("Use IoPin::writeActive/writeInactive instead")]]
  virtual uint8_t pinOnValue();
  [[deprecated("Use IoPin::writeActive/writeInactive instead")]]
  virtual uint8_t pinOffValue();
  virtual void turnOn(_supla_int_t duration = 0);
  virtual void turnOff(_supla_int_t duration = 0);
  virtual bool isOn();
  virtual void toggle(_supla_int_t duration = 0);

  void attach(Supla::Control::Button *);

  void handleAction(int event, int action) override;

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void purgeConfig() override;
  void onInit() override;
  void onLoadState() override;
  void onSaveState() override;
  void iterateAlways() override;
  bool iterateConnected() override;
  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) override;
  void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) override;
  Supla::ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                              bool local = false) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;
  /**
   * Returns remaining countdown timer time in seconds for an active relay
   * countdown timer.
   *
   * Returns false when countdown timer support is disabled or the timer is not
   * active.
   */
  bool getRemainingCountdownTimerSec(uint32_t *remainingSec) const override;

  // Method is used by external integrations to prepare TSD_SuplaChannelNewValue
  // value for specific channel type (i.e. to prefill durationMS field when
  // required)
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;

  unsigned _supla_int_t getStoredTurnOnDurationMs();
  void setStoredTurnOnDurationMs(uint32_t durationMs);

  bool isStaircaseFunction(uint32_t functionToCheck = 0) const;
  bool isImpulseFunction(uint32_t functionToCheck = 0) const;
  void disableCountdownTimerFunction();
  void enableCountdownTimerFunction();
  bool isCountdownTimerFunctionEnabled() const;
  void setMinimumAllowedDurationMs(uint32_t durationMs);

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

  /**
   * Set default duration for staircase timer function
   *
   * @param durationMs
   */
  void setDefaultStaircaseDurationMs(uint16_t durationMs) {
    defaultStaircaseDurationMs = durationMs;
  }

  /**
   * Set default duration for impulse functions (controlling gate, door etc.)
   *
   * @param durationMs
   */
  void setDefaultImpulseDurationMs(uint16_t durationMs) {
    defaultImpulseDurationMs = durationMs;
  }

  bool setRuntimeFunction(uint32_t channelFunction) override;
  bool setAndSaveFunction(uint32_t channelFunction) override;

  /**
   * Set restart timer on toggle for staircase and impulse functions
   *
   * @param restart true to restart timer on toggle, false to not
   */
  void setRestartTimerOnToggle(bool restart);

  /**
   * Get restart timer on toggle for staircase and impulse functions
   *
   * @return true if restart timer on toggle, false if not
   */
  bool isRestartTimerOnToggle() const;

  bool isFullyInitialized() const;

  void enableCyclicMode(uint32_t turnOnTimeMs, uint32_t turnOffTimeMs);
  void disableCyclicMode();
  bool isCyclicMode() const;

 protected:
  Relay(Supla::Io::IoPin outputPin,
        _supla_int_t functions,
        Supla::Channel &externalChannel,
        ElementMode mode);

  struct ButtonListElement {
    Supla::Control::Button *button = nullptr;
    ButtonListElement *next = nullptr;
  };

  void applyDuration(int durationMs, bool turnOn);

  virtual void setNewChannelValue(bool value);

  void saveConfig() const;
  void loadRelayConfigOnly();
  void purgeRelayConfigOnly();
  void updateTimerValue();
  void emitCountdownTimerActionIfNeeded();
  void updateRelayHvacAggregator();
  uint32_t durationMs = 0;
  uint32_t storedTurnOnDurationMs = 0;
  uint32_t durationTimestamp = 0;
  uint32_t turnOffDurationForCycle = 0;
  uint16_t defaultStaircaseDurationMs = 10000;
  uint16_t defaultImpulseDurationMs = 500;

  uint32_t overcurrentThreshold = 0;   // 0.01 A
  uint32_t overcurrentMaxAllowed = 0;  // 0.01 A
  uint32_t overcurrentActiveTimestamp = 0;
  uint32_t overcurrentCheckTimestamp = 0;

  uint32_t timerUpdateTimestamp = 0;
  uint32_t lastCountdownTimerRemainingSec = UINT32_MAX;
  uint32_t postponeCommTimestamp = 0;

  ButtonListElement *buttonList = nullptr;

  uint16_t minimumAllowedDurationMs = 0;
  int16_t defaultRelatedMeterChannelNo = -1;

  bool keepTurnOnDurationMs = false;
  bool turnOffWhenEmptyAggregator = true;
  bool initDone = false;
  bool restartTimerOnToggle = false;
  bool skipInitialStateSetting = false;
  bool preloadStateOnSoftReset = false;

  int8_t stateOnInit = STATE_ON_INIT_OFF;
  Supla::Io::IoPin outputPin;

  static int16_t relayStorageSaveDelay;
};

};  // namespace Control
};  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_H_
