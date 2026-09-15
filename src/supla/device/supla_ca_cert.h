// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_SUPLA_CA_CERT_H_
#define SRC_SUPLA_DEVICE_SUPLA_CA_CERT_H_

#ifdef ARDUINO
#include <Arduino.h>
#else
#define PROGMEM
#endif

// CA used for public Supla servers
extern const char suplaCACert[] PROGMEM;

// CA used for private Supla servers signed by Supla root CA
extern const char supla3rdCACert[] PROGMEM;

#endif  // SRC_SUPLA_DEVICE_SUPLA_CA_CERT_H_
