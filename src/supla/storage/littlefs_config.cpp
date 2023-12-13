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

#include <supla/storage/key_value.h>
#include <supla/log_wrapper.h>
#include <LittleFS.h>
#include <string.h>
#include "littlefs_config.h"

namespace Supla {
  const char ConfigFileName[] = "/supla-dev.cfg";
  const char CustomCAFileName[] = "/custom_ca.pem";
};


#define BIG_BLOG_SIZE_TO_BE_STORED_IN_FILE 32

Supla::LittleFsConfig::LittleFsConfig(int configMaxSize)
    : configMaxSize(configMaxSize) {
}

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

    SUPLA_LOG_DEBUG("LittleFsConfig: config file size %d", fileSize);
    if (fileSize > configMaxSize) {
      SUPLA_LOG_ERROR("LittleFsConfig: config file is too big");
      cfg.close();
      LittleFS.end();
      return false;
    }

    uint8_t *buf = new uint8_t[configMaxSize];
    if (buf == nullptr) {
      SUPLA_LOG_ERROR("LittleFsConfig: failed to allocate memory");
      cfg.close();
      LittleFS.end();
      return false;
    }

    memset(buf, 0, configMaxSize);
    int bytesRead = cfg.read(buf, fileSize);

    cfg.close();
    LittleFS.end();
    if (bytesRead != fileSize) {
      SUPLA_LOG_DEBUG(
          "LittleFsConfig: read bytes %d, while file is %d bytes",
          bytesRead,
          fileSize);
      delete []buf;
      return false;
    }

    SUPLA_LOG_DEBUG("LittleFsConfig: initializing storage from file...");
    auto result =  initFromMemory(buf, fileSize);
    SUPLA_LOG_DEBUG("LittleFsConfig: init result %d", result);
    delete []buf;
    return result;
  } else {
    SUPLA_LOG_DEBUG("LittleFsConfig:: config file missing");
  }
  LittleFS.end();
  return true;
}

void Supla::LittleFsConfig::commit() {
  uint8_t *buf = new uint8_t[configMaxSize];
  if (buf == nullptr) {
    SUPLA_LOG_ERROR("LittleFsConfig: failed to allocate memory");
    return;
  }

  memset(buf, 0, configMaxSize);

  size_t dataSize = serializeToMemory(buf, configMaxSize);

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
  delete []buf;
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

  File suplaDir = LittleFS.open("/supla", "r");
  if (suplaDir && suplaDir.isDirectory()) {
    File file = suplaDir.openNextFile();
    while (file) {
      if (!file.isDirectory()) {
        SUPLA_LOG_DEBUG("LittleFsConfig: removing file /supla/%s", file.name());
        char path[200] = {};
        snprintf(path, sizeof(path), "/supla/%s", file.name());
        file.close();
        if (!LittleFS.remove(path)) {
          SUPLA_LOG_ERROR("LittleFsConfig: failed to remove file");
        }
      }
      file = suplaDir.openNextFile();
    }
  } else {
    SUPLA_LOG_DEBUG("LittleFsConfig: failed to open supla directory");
  }

  LittleFS.end();

  Supla::KeyValue::removeAll();
}

bool Supla::LittleFsConfig::setBlob(const char* key,
                                    const char* value,
                                    size_t blobSize) {
  if (blobSize < BIG_BLOG_SIZE_TO_BE_STORED_IN_FILE) {
    return Supla::KeyValue::setBlob(key, value, blobSize);
  }

  SUPLA_LOG_DEBUG("LittleFS: writing file %s", key);
  if (!initLittleFs()) {
    return false;
  }

  LittleFS.mkdir("/supla");

  char filename[50] = {};
  snprintf(filename, sizeof(filename), "/supla/%s", key);
  File file = LittleFS.open(filename, "w");
  if (!file) {
    SUPLA_LOG_ERROR(
        "LittleFsConfig: failed to open blob file \"%s\" for write", key);
    LittleFS.end();
    return false;
  }

  file.write(reinterpret_cast<const uint8_t*>(value), blobSize);
  file.close();
  LittleFS.end();
  return true;
}

bool Supla::LittleFsConfig::getBlob(const char* key,
                                    char* value,
                                    size_t blobSize) {
  if (blobSize < BIG_BLOG_SIZE_TO_BE_STORED_IN_FILE) {
    return Supla::KeyValue::getBlob(key, value, blobSize);
  }

  if (!initLittleFs()) {
    return false;
  }

  char filename[50] = {};
  snprintf(filename, sizeof(filename), "/supla/%s", key);
  File file = LittleFS.open(filename, "r");
  if (!file) {
    SUPLA_LOG_ERROR(
        "LittleFsConfig: failed to open blob file \"%s\" for read", key);
    LittleFS.end();
    return false;
  }
  size_t fileSize = file.size();
  if (fileSize > blobSize) {
    SUPLA_LOG_ERROR("LittleFsConfig: blob file is too big");
    file.close();
    LittleFS.end();
    return false;
  }

  int bytesRead = file.read(reinterpret_cast<uint8_t*>(value), fileSize);

  file.close();
  LittleFS.end();
  return bytesRead == fileSize;
}

int Supla::LittleFsConfig::getBlobSize(const char* key) {
  if (!initLittleFs()) {
    return false;
  }

  char filename[50] = {};
  snprintf(filename, sizeof(filename), "/supla/%s", key);
  File file = LittleFS.open(filename, "r");
  if (!file) {
    SUPLA_LOG_ERROR(
        "LittleFsConfig: failed to open blob file \"%s\"", key);
    LittleFS.end();
    return false;
  }
  int fileSize = file.size();

  file.close();
  LittleFS.end();
  return fileSize;
}


#endif  // !defined(ARDUINO_ARCH_AVR)
#endif  // SUPLA_EXCLUDE_LITTLEFS_CONFIG
