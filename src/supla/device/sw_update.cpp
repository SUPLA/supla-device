// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sw_update.h"

#include <string.h>
#include <supla/storage/config.h>

#if !defined(SUPLA_TEST)
#if defined(ARDUINO) || defined(SUPLA_LINUX) || defined(SUPLA_FREERTOS)
// TODO(klew): implement sw update for remaining targets
Supla::Device::SwUpdate *Supla::Device::SwUpdate::Create(
    SuplaDeviceClass *sdc, const char *newUrl, Supla::SwUpdateMode mode) {
  (void)(newUrl);
  (void)(sdc);
  (void)(mode);
  return nullptr;
}
#endif
#endif  // !SUPLA_TEST

Supla::Device::SwUpdate::~SwUpdate() {
  if (newVersion) {
    delete[] newVersion;
    newVersion = nullptr;
  }
  if (updateUrl) {
    delete[] updateUrl;
    updateUrl = nullptr;
  }
  if (changelogUrl) {
    delete[] changelogUrl;
    changelogUrl = nullptr;
  }
}

Supla::Device::SwUpdate::SwUpdate(SuplaDeviceClass *sdc,
                                  const char *newUrl,
                                  Supla::SwUpdateMode mode)
    : sdc(sdc), mode(mode) {
  setUrl(newUrl);
}

void Supla::Device::SwUpdate::setUrl(const char *newUrl) {
  if (newUrl) {
    strncpy(url, newUrl, SUPLA_MAX_URL_LENGTH - 1);
    url[SUPLA_MAX_URL_LENGTH - 1] = '\0';
  }
}
