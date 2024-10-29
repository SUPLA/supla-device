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

#include "esp_idf_watchdog.h"

#include <SuplaDevice.h>
#include <supla/events.h>
#include <esp_task_wdt.h>

using Supla::Watchdog;

Watchdog::Watchdog(SuplaDeviceClass *sdc) {
  this->sdc = sdc;
  reconfigureTimeoutMs(60000);
  ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
  ESP_ERROR_CHECK(esp_task_wdt_status(NULL));

  sdc->addAction(0, this, Supla::ON_DEVICE_STATUS_CHANGE);
}

void Watchdog::iterateAlways() {
  esp_task_wdt_reset();
}

void Watchdog::handleAction(int event, int action) {
  if (sdc->getCurrentStatus() == STATUS_SW_DOWNLOAD ||
      sdc->getDeviceMode() == Supla::DEVICE_MODE_TEST) {
    reconfigureTimeoutMs(5 * 60000);  // 5 min
  } else {
    reconfigureTimeoutMs(60000);  // 60 s
  }
}

void Watchdog::reconfigureTimeoutMs(uint32_t timeoutMs) {
  if (lastTimetoutMs == timeoutMs) {
    return;
  }
  lastTimetoutMs = timeoutMs;
  SUPLA_LOG_INFO("Reconfiguring watchdog timeout to %d ms", timeoutMs);
  esp_task_wdt_config_t twdt_config = {
      .timeout_ms = timeoutMs,
      .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,  // Bitmask of all cores
      .trigger_panic = true,
  };
  esp_task_wdt_reconfigure(&twdt_config);
}

