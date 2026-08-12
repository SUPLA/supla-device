// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
