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

#include "sw_update.h"

#include <supla/storage/config.h>
#include <string.h>

#if !defined(SUPLA_TEST)
#if defined(ARDUINO) || defined(SUPLA_LINUX) || \
    defined(SUPLA_FREERTOS)
// TODO(klew): implement sw update for remaining targets
Supla::Device::SwUpdate *Supla::Device::SwUpdate::Create(SuplaDeviceClass *sdc,
                                                         const char *newUrl) {
  (void)(newUrl);
  (void)(sdc);
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

Supla::Device::SwUpdate::SwUpdate(SuplaDeviceClass *sdc, const char *newUrl)
    : sdc(sdc) {
  setUrl(newUrl);
}

void Supla::Device::SwUpdate::setUrl(const char *newUrl) {
  if (newUrl) {
    strncpy(url, newUrl, SUPLA_MAX_URL_LENGTH - 1);
    url[SUPLA_MAX_URL_LENGTH - 1] = '\0';
  }
}


