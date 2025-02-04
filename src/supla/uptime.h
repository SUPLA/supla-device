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

#ifndef SRC_SUPLA_UPTIME_H_
#define SRC_SUPLA_UPTIME_H_

#include <stdint.h>

namespace Supla {

class Uptime {
 public:
  Uptime();

  void iterate(uint32_t millis);
  void resetConnectionUptime();
  void setConnectionLostCause(uint8_t cause);

  uint32_t getUptime() const;
  uint32_t getConnectionUptime() const;
  uint8_t getLastResetCause() const;

 protected:
  uint32_t lastMillis = 0;
  uint32_t deviceUptime = 0;
  uint32_t connectionUptime = 0;
  uint8_t lastConnectionResetCause = 0;
  bool acceptConnectionLostCause = false;
};

};  // namespace Supla

#endif  // SRC_SUPLA_UPTIME_H_
