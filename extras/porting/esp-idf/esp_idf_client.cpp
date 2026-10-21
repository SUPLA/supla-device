// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_idf_client.h"

Supla::Client *Supla::ClientBuilder() { return new Supla::EspIdfClient; }
