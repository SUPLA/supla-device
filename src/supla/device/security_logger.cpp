// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "security_logger.h"

#include <supla/log_wrapper.h>
#include <string.h>
#include <stdio.h>
#include <time.h>

using Supla::Device::SecurityLogger;
using Supla::SecurityLogEntry;

char SecurityLogger::buffer[SUPLA_SECURITY_LOG_ENTRY_SIZE] = {};

SecurityLogger::SecurityLogger() {}
SecurityLogger::~SecurityLogger() {}

void SecurityLogger::log(uint32_t source, const char *log) {
  SecurityLogEntry entry = {};
  const size_t logSize = sizeof(entry.log);
  if (strnlen(log, logSize) == logSize) {
    SUPLA_LOG_WARNING("SecurityLogger: log too long");
  }
  entry.source = source;
  strncpy(entry.log, log, logSize - 1);
  entry.log[logSize - 1] = '\0';
  entry.index = ++index;
  entry.timestamp = time(nullptr);
  entry.print();
  storeLog(entry);
}

void SecurityLogger::deleteAll() {
  index = 0;
}

Supla::SecurityLogEntry *SecurityLogger::getLog() {
  return nullptr;
}


bool SecurityLogger::prepareGetLog() {
  return false;
}

void SecurityLogger::storeLog(const SecurityLogEntry &) {
}

const char *SecurityLogger::getSourceName(uint32_t source) {
  if (source < 10) {
    // Predefined source
    switch (static_cast<Supla::SecurityLogSource>(source)) {
      case Supla::SecurityLogSource::NONE: {
        return "Unknown";
        break;
      }
      case Supla::SecurityLogSource::LOCAL_DEVICE: {
        return "Local";
        break;
      }
      case Supla::SecurityLogSource::REMOTE: {
        return "Remote";
        break;
      }
      default: {
        snprintf(buffer,
                 sizeof(buffer) - 1,
                 "Unknown(%d)",
                 static_cast<int>(source));
        break;
      }
    }
  } else {
    // IP
    uint8_t sourceBytes[4] = {};
    memcpy(sourceBytes, &source, sizeof(sourceBytes));
    snprintf(buffer,
             sizeof(buffer) - 1,
             "%d.%d.%d.%d",
             sourceBytes[3],
             sourceBytes[2],
             sourceBytes[1],
             sourceBytes[0]);
  }

  return buffer;
}

void SecurityLogEntry::print() const {
  SUPLA_LOG_INFO("SSLOG: %d.[%d][%s] %.*s",
                 index,
                 timestamp,
                 Supla::Device::SecurityLogger::getSourceName(source),
                 static_cast<int>(sizeof(log)),
                 log);
}

bool SecurityLogger::isEnabled() const {
  return false;
}

void SecurityLogger::init() {
}

bool Supla::SecurityLogEntry::isEmpty() const {
  bool empty = true;
  for (size_t i = 0; i < SUPLA_SECURITY_LOG_ENTRY_SIZE; i++) {
    if (rawData[i] != 0xFF) {
      empty = false;
      break;
    }
  }
  return empty;
}
