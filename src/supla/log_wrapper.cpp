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

#ifdef ARDUINO
#include <Arduino.h>

#include <supla-common/log.h>
#include "log_wrapper.h"

extern "C" void serialPrintLn(const char *);

void serialPrintLn(const char *message) {
  Serial.println(message);
}

void supla_logf(int __pri, const __FlashStringHelper *__fmt, ...) {
  va_list ap;
  char *buffer = NULL;
  int size = 0;

  if (__fmt == NULL) return;

  String fmt(__fmt);


  while (1) {
    va_start(ap, __fmt);
    if (0 == supla_log_string(&buffer, &size, ap, fmt.c_str())) {
      va_end(ap);
      break;
    } else {
      va_end(ap);
    }
    va_end(ap);
  }

  if (buffer == NULL) return;

  supla_vlog(__pri, buffer);
  free(buffer);
}

#endif /*ARDUINO*/
