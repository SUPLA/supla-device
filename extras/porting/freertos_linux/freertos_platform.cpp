// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <supla/tools.h>

void deviceSoftwareReset() {
  // TODO(klew): implement device sw reset for freeRTOS
}

bool isDeviceSoftwareResetSupported() {
  return false;
}

bool isLastResetSoft() {
  // TODO(klew): implement
  return false;
}

bool Supla::isLastResetPower() {
  // TODO(klew): implement
  return false;
}

int Supla::getPlatformId() {
  // TODO(klew): implement when needed
  return 0;
}
