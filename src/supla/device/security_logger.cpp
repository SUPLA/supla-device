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

#include "security_logger.h"

#include <supla/log_wrapper.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

using Supla::Device::SecurityLogger;
using Supla::SecurityLogEntry;

SecurityLogger::SecurityLogger() {}
SecurityLogger::~SecurityLogger() {}

void SecurityLogger::log(uint32_t source, const char *log) {
  SecurityLogEntry entry;
  entry.source = source;
  strncpy(entry.log, log, sizeof(entry.log));
  entry.index = ++index;
  entry.timestamp = time(nullptr);
  entry.print();
  storeLog(entry);
}

void SecurityLogger::deleteAll() {
  index = 0;
  // TODO(klew): implement
}

char *SecurityLogger::getLog() {
  return nullptr;
}


bool SecurityLogger::prepareGetLog() {
  return false;
}

void SecurityLogger::storeLog(const SecurityLogEntry &) {
}

void SecurityLogEntry::print() const {
  char sourceText[32] = {};
  if (source < 10) {
    // Predefined source
    switch (static_cast<Supla::SecurityLogSource>(source)) {
      case Supla::SecurityLogSource::NONE: {
        snprintf(sourceText, sizeof(sourceText) - 1, "Unknown");
        break;
      }
      case Supla::SecurityLogSource::LOCAL_DEVICE: {
        snprintf(sourceText, sizeof(sourceText) - 1, "Local");
        break;
      }
      case Supla::SecurityLogSource::REMOTE: {
        snprintf(sourceText, sizeof(sourceText) - 1, "Remote");
        break;
      }
      default: {
        snprintf(sourceText,
                 sizeof(sourceText) - 1,
                 "Unknown(%d)",
                 static_cast<int>(source));
        break;
      }
    }
  } else {
    // IP
    snprintf(sourceText,
             sizeof(sourceText) - 1,
             "%d.%d.%d.%d",
             sourceBytes[3],
             sourceBytes[2],
             sourceBytes[1],
             sourceBytes[0]);
  }
  SUPLA_LOG_WARNING("***************************");
  SUPLA_LOG_INFO("SSLOG: %d.[%d][%s] %s", index, timestamp, sourceText, log);
  SUPLA_LOG_WARNING("***************************");
}

