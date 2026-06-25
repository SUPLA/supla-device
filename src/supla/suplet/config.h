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

#ifndef SRC_SUPLA_SUPLET_CONFIG_H_
#define SRC_SUPLA_SUPLET_CONFIG_H_

#ifndef SUPLA_SUPLET_ENABLED
#if defined(SUPLA_TEST)
#define SUPLA_SUPLET_ENABLED 1
#else
#define SUPLA_SUPLET_ENABLED 0
#endif
#endif

#ifndef SUPLA_SUPLET_MAX_INSTANCES
#define SUPLA_SUPLET_MAX_INSTANCES 8
#endif

#ifndef SUPLA_SUPLET_MAX_INSTANCE_ID
#define SUPLA_SUPLET_MAX_INSTANCE_ID 255
#endif

#ifndef SUPLA_SUPLET_MAX_CONFIG_SIZE
#define SUPLA_SUPLET_MAX_CONFIG_SIZE 512
#endif

#ifndef SUPLA_SUPLET_MAX_CACHED_DEFINITIONS
#define SUPLA_SUPLET_MAX_CACHED_DEFINITIONS 16
#endif

#ifndef SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE
#if defined(ESP8266) || defined(ARDUINO_ARCH_ESP8266) || \
    defined(SUPLA_DEVICE_ESP8266)
#define SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE 16384
#else
#define SUPLA_SUPLET_MAX_DEFINITION_JSON_SIZE 24576
#endif
#endif

#ifndef SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE
#define SUPLA_SUPLET_DEFINITION_CACHE_CHUNK_SIZE 2048
#endif

#ifndef SUPLA_SUPLET_CALCFG_SESSION_TIMEOUT_MS
#define SUPLA_SUPLET_CALCFG_SESSION_TIMEOUT_MS 30000
#endif

#endif  // SRC_SUPLA_SUPLET_CONFIG_H_
