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

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_WATCHDOG_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_WATCHDOG_H_

#include <supla/element.h>
#include <supla/action_handler.h>

class SuplaDeviceClass;

namespace Supla {
class Watchdog : public Supla::Element, public Supla::ActionHandler {
 public:
  explicit Watchdog(SuplaDeviceClass *sdc);
  void iterateAlways() override;
  void handleAction(int event, int action) override;
  void reconfigureTimeoutMs(uint32_t timeoutMs);

 private:
  SuplaDeviceClass *sdc = nullptr;
  uint32_t lastTimetoutMs = 0;
};
}  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_WATCHDOG_H_
