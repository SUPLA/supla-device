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

#ifndef SUPLA_EXCLUDE_LITTLEFS_CONFIG

#if !defined(ARDUINO_ARCH_AVR)
// don't compile it on Arduino Mega

#include <LittleFS.h>
#include "littlefs_config.h"
#include <supla/log_wrapper.h>

namespace Supla {
  const char ConfigFileName[] = "/supla-dev.cfg";
  const char CustomCAFileName[] = "/custom_ca.pem";
};

#define SUPLA_LITTLEFS_CONFIG_BUF_SIZE 1024

Supla::LittleFsConfig::LittleFsConfig() {}

Supla::LittleFsConfig::~LittleFsConfig() {}

bool Supla::LittleFsConfig::init() {
  if (first) {
    SUPLA_LOG_WARNING(
        "LittleFsConfig: init called on non empty database. Aborting");
    // init can be done only on empty storage
    return false;
  }

  if (!initLittleFs()) {
    return false;
  }

  if (LittleFS.exists(ConfigFileName)) {
    File cfg = LittleFS.open(ConfigFileName, "r");
    if (!cfg) {
      SUPLA_LOG_ERROR("LittleFsConfig: failed to open config file");
      LittleFS.end();
      return false;
    }

    int fileSize = cfg.size();

    if (fileSize > SUPLA_LITTLEFS_CONFIG_BUF_SIZE) {
      SUPLA_LOG_ERROR("LittleFsConfig: config file is too big");
      cfg.close();
      LittleFS.end();
      return false;
    }

    uint8_t buf[SUPLA_LITTLEFS_CONFIG_BUF_SIZE] = {};
    int bytesRead = cfg.read(buf, fileSize);

    cfg.close();
    LittleFS.end();
    if (bytesRead != fileSize) {
      SUPLA_LOG_DEBUG(
          "LittleFsConfig: read bytes %d, while file is %d bytes",
          bytesRead,
          fileSize);
      return false;
    }

    SUPLA_LOG_DEBUG("LittleFsConfig: initializing storage from file...");
    return initFromMemory(buf, fileSize);
  } else {
    SUPLA_LOG_DEBUG("LittleFsConfig:: config file missing");
  }
  LittleFS.end();
  return true;
}

void Supla::LittleFsConfig::commit() {
  uint8_t buf[SUPLA_LITTLEFS_CONFIG_BUF_SIZE] = {};

  size_t dataSize = serializeToMemory(buf, SUPLA_LITTLEFS_CONFIG_BUF_SIZE);

  if (!initLittleFs()) {
    return;
  }

  File cfg = LittleFS.open(ConfigFileName, "w");
  if (!cfg) {
    SUPLA_LOG_ERROR("LittleFsConfig: failed to open config file for write");
    LittleFS.end();
    return;
  }

  cfg.write(buf, dataSize);
  cfg.close();
  LittleFS.end();
}

bool Supla::LittleFsConfig::getCustomCA(char* customCA, int maxSize) {
  if (!initLittleFs()) {
    return false;
  }

  if (LittleFS.exists(CustomCAFileName)) {
    File file = LittleFS.open(CustomCAFileName, "r");
    if (!file) {
      SUPLA_LOG_ERROR("LittleFsConfig: failed to open custom CA file");
      LittleFS.end();
      return false;
    }

    int fileSize = file.size();

    if (fileSize > maxSize) {
      SUPLA_LOG_ERROR("LittleFsConfig: custom CA file is too big");
      file.close();
      LittleFS.end();
      return false;
    }

    int bytesRead = file.read(reinterpret_cast<uint8_t *>(customCA), fileSize);

    file.close();
    LittleFS.end();
    if (bytesRead != fileSize) {
      SUPLA_LOG_DEBUG(
          "LittleFsConfig: read bytes %d, while file is %d bytes",
          bytesRead,
          fileSize);
      return false;
    }

    return true;
  } else {
    SUPLA_LOG_DEBUG("LittleFsConfig:: custom ca file missing");
  }
  LittleFS.end();
  return true;
}

int Supla::LittleFsConfig::getCustomCASize() {
  if (!initLittleFs()) {
    return 0;
  }

  if (LittleFS.exists(CustomCAFileName)) {
    File file = LittleFS.open(CustomCAFileName, "r");
    if (!file) {
      SUPLA_LOG_ERROR("LittleFsConfig: failed to open custom CA file");
      LittleFS.end();
      return false;
    }

    int fileSize = file.size();

    file.close();
    LittleFS.end();
    return fileSize;
  }
  return 0;
}

bool Supla::LittleFsConfig::setCustomCA(const char* customCA) {
  size_t dataSize = strlen(customCA);

  if (!initLittleFs()) {
    return false;
  }

  File file = LittleFS.open(CustomCAFileName, "w");
  if (!file) {
    SUPLA_LOG_ERROR(
        "LittleFsConfig: failed to open custom CA file for write");
    LittleFS.end();
    return false;
  }

  file.write(reinterpret_cast<const uint8_t*>(customCA), dataSize);
  file.close();
  LittleFS.end();
  return true;
}

bool Supla::LittleFsConfig::initLittleFs() {
  bool result = LittleFS.begin();
  if (!result) {
    SUPLA_LOG_WARNING("LittleFsConfig: formatting partition");
    LittleFS.format();
    result = LittleFS.begin();
    if (!result) {
      SUPLA_LOG_ERROR(
          "LittleFsConfig: failed to mount and to format partition");
    }
  }

  return result;
}

void Supla::LittleFsConfig::removeAll() {
  SUPLA_LOG_DEBUG("LittleFsConfig remove all called");

  if (!initLittleFs()) {
    return;
  }
  LittleFS.remove(CustomCAFileName);
  LittleFS.end();

  Supla::KeyValue::removeAll();
}

#endif
#endif  // SUPLA_EXCLUDE_LITTLEFS_CONFIG
