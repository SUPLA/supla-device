// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEFINITIONS_H_
#define SRC_SUPLA_DEFINITIONS_H_

#ifdef ARDUINO
#include <Arduino.h>
#endif

#ifndef LSBFIRST
#define LSBFIRST 0
#endif /*LSBFIRST*/

#ifndef LOW
#define LOW 0
#endif /*LOW*/

#ifndef HIGH
#define HIGH 1
#endif /*HIGH*/

#ifndef INPUT
#define INPUT 0
#endif /*INPUT*/

#ifndef INPUT_PULLUP
#define INPUT_PULLUP 2
#endif /*INPUT_PULLUP*/

#ifndef OUTPUT
#define OUTPUT 1
#endif /*OUTPUT*/

#ifndef RISING
#define RISING    0x01
#endif /*RISING*/

#ifndef FALLING
#define FALLING   0x02
#endif /*FALLING*/

#ifndef CHANGE
#define CHANGE    0x03
#endif /*CHANGE*/

#ifndef ONLOW
#define ONLOW     0x04
#endif /*ONLOW*/

#ifndef ONHIGH
#define ONHIGH    0x05
#endif /*ONHIGH*/

#ifndef ONLOW_WE
#define ONLOW_WE  0x0C
#endif /*ONLOW_WE*/

#ifndef ONHIGH_WE
#define ONHIGH_WE 0x0D
#endif /*ONHIGH_WE*/

#endif  // SRC_SUPLA_DEFINITIONS_H_
