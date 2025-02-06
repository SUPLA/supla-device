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
  uint8_t getValueOpenStateFromDevice() const override;

 protected:
  uint8_t valveOpenState = 0;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_VIRTUAL_VALVE_H_
