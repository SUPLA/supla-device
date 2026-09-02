// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
