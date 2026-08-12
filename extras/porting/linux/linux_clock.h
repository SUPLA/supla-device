// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_LINUX_CLOCK_H_
#define EXTRAS_PORTING_LINUX_LINUX_CLOCK_H_

#include <supla/clock/clock.h>

namespace Supla {
class LinuxClock : public Clock {
 public:
  LinuxClock();

  void onTimer() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
};
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_LINUX_CLOCK_H_
