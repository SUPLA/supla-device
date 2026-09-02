// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_VIRTUAL_BINARY_H_
#define SRC_SUPLA_SENSOR_VIRTUAL_BINARY_H_

#include <supla/sensor/binary_base.h>

#include "../action_handler.h"

namespace Supla {
namespace Sensor {
/**
 * Virtual binary sensor with explicit logical state control.
 *
 * The class stores the logical state internally and mirrors it to the channel
 * value taking server inverted logic into account. It can also auto-clear the
 * state after the configured timeout, unless timeout handling is disabled with
 * setUseConfiguredTimeout(false).
 */
class VirtualBinary : public BinaryBase, public ActionHandler {
 public:
  explicit VirtualBinary(bool keepStateInStorage = false);
  bool getValue() override;
  void onInit() override;
  void iterateAlways() override;
  void handleAction(int event, int action) override;
  void onLoadState() override;
  void onSaveState() override;

  /** Sets the logical state to ON. */
  void set();
  /** Sets the logical state to OFF. */
  void clear();
  /** Toggles the logical state. */
  void toggle();
  /**
   * Enables or disables timeout auto-application from the configured channel
   * timeout value.
   *
   * When disabled, the timeout value is still available via getters, but the
   * sensor will not auto-clear itself in iterateAlways().
   */
  void setUseConfiguredTimeout(bool useConfiguredTimeout);

  /**
   * Configures whether the logical state should be persisted in storage.
   *
   * This should be set only before SuplaDevice.begin(). Changing it later can
   * break the stored state layout.
   */
  void setKeepStateInStorage(bool);

 protected:
  void setLogicalState(bool logicalState, bool fromTimeout = false);

  bool state = false;
  bool keepStateInStorage = false;
  uint32_t stateChangeTimeMs = 0;
  bool clearedByTimeout = false;
  bool useConfiguredTimeout = true;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_VIRTUAL_BINARY_H_
