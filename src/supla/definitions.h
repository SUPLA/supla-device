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
