// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "spiffs_storage.h"

#include <esp_spiffs.h>
#include <stdio.h>
#include <string.h>
#include <supla/log_wrapper.h>

#include <cstdlib>

namespace Supla {

// By default, update storage every 3 min
// Storage is not saved if there is no change
#define SUPLA_WRITING_PERIOD 3 * 60 * 1000L

SpiffsStorage::SpiffsStorage(uint32_t storageSize)
    : Storage(0), bufferSize(storageSize) {
  setStateSavePeriod(SUPLA_WRITING_PERIOD);

  buffer = reinterpret_cast<char *>(malloc(bufferSize));
  memset(buffer, 0, bufferSize);
}

SpiffsStorage::~SpiffsStorage() {
  if (buffer) {
    free(buffer);
  }
  esp_vfs_spiffs_unregister(NULL);
}

bool SpiffsStorage::init() {
  if (!buffer) {
    SUPLA_LOG_ERROR("Storage: failed to allocate storage memory");
    return false;
  }
  if (!spiffsInitDone) {
    // init spiffs
    esp_vfs_spiffs_conf_t conf = {.base_path = "/spiffs",
                                  .partition_label = NULL,
                                  .max_files = 5,
                                  .format_if_mount_failed = true};

    esp_err_t ret = esp_vfs_spiffs_register(&conf);

    if (ret != ESP_OK) {
      if (ret == ESP_FAIL) {
        SUPLA_LOG_WARNING("Failed to mount or format filesystem");
      } else if (ret == ESP_ERR_NOT_FOUND) {
        SUPLA_LOG_ERROR("Failed to find SPIFFS partition");
      } else {
        SUPLA_LOG_ERROR("Failed to initialize SPIFFS (%s)",
                        esp_err_to_name(ret));
      }
      return false;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to get SPIFFS partition information (%s)",
                      esp_err_to_name(ret));
    } else {
      SUPLA_LOG_INFO("Partition size: total: %d, used: %d", total, used);
    }

    spiffsInitDone = true;
  }

  FILE *file = fopen("/spiffs/storage.bin", "rb");

  if (file) {
    fseek(file, 0, SEEK_END);
    uint32_t fileSize = ftell(file);
    rewind(file);

    if (fileSize != bufferSize) {
      SUPLA_LOG_WARNING(
          "Storage: storage file size differs from configured buffer size. "
          "Rewritting...");
    } else {
      SUPLA_LOG_DEBUG("Storage: reading file to internal buffer");
      fread(buffer, 1, bufferSize, file);
    }

    fclose(file);
  }

  return Storage::init();
}

int SpiffsStorage::readStorage(unsigned int offset,
                               unsigned char *buf,
                               unsigned int size,
                               bool logs) {
  if (!buffer) {
    return 0;
  }

  if (offset + size > bufferSize) {
    SUPLA_LOG_ERROR("Storage: attempt to read out of storage size");
    return 0;
  }

  for (int i = 0; i < size; i++) {
    buf[i] = buffer[offset + i];
  }
  return size;
}

int SpiffsStorage::writeStorage(unsigned int offset,
                                const unsigned char *buf,
                                unsigned int size) {
  dataChanged = true;

  if (offset + size > bufferSize) {
    SUPLA_LOG_ERROR("Storage: attempt to write out of storage size");
    return 0;
  }

  for (int i = 0; i < size; i++) {
    buffer[offset + i] = buf[i];
  }

  return size;
}

void SpiffsStorage::commit() {
  if (dataChanged) {
    // write to file
    SUPLA_LOG_DEBUG("Storage: datachange, commit");
    FILE *file = fopen("/spiffs/storage.bin", "wb");
    if (file) {
      uint32_t bytesWritten = fwrite(buffer, 1, bufferSize, file);
      if (bufferSize != bytesWritten) {
        SUPLA_LOG_ERROR("Storage: file write error");
      } else {
        SUPLA_LOG_DEBUG("Storage: file saved");
      }

      fclose(file);
    } else {
      SUPLA_LOG_ERROR("Storage: failed to open storage file");
    }
  }
  dataChanged = false;
}

}  // namespace Supla
