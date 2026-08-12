// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_CONTROL_VIRTUAL_VALVE_H_
#define SRC_SUPLA_CONTROL_VIRTUAL_VALVE_H_

#include "valve_base.h"

namespace Supla {
namespace Control {

/**
 * VirtualValve implements Valve as a "device" which simulates a valve
 * in memory.
 * It will report state based on the value set by setValueOnDevice.
 */
class VirtualValve : public ValveBase {
 public:
   /**
    * Constructor
    *
    * @param openClose true = open/close, false = 0-100 percentage
    */
  explicit VirtualValve(bool openClose = true);

  /**
   * Sets the value of the valve virtual device
   *
   * @param openLevel 0-100, 0 = closed, >= 1 = open
   */
  void setValueOnDevice(uint8_t openLevel) override;

  /**
   * Returns the value of the valve virtual device
   *
   * @return 0-100, 0 = closed, >= 1 = open
   */
  uint8_t getValueOpenStateFromDevice() override;

 protected:
  uint8_t valveOpenState = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_VIRTUAL_VALVE_H_
