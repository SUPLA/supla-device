// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECURITY_LOGGER_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECURITY_LOGGER_H_

#include <supla/device/security_logger.h>
#include <esp_partition.h>
#include <supla/network/html/security_log_list.h>

class ESPIdfSecurityLogger : public Supla::Device::SecurityLogger {
 public:
  ESPIdfSecurityLogger();
  ~ESPIdfSecurityLogger() override;

  void init() override;
  bool isEnabled() const override;
  void deleteAll() override;

  Supla::SecurityLogEntry *getLog() override;
  bool prepareGetLog() override;

  void storeLog(const Supla::SecurityLogEntry &entry) override;

 private:
  void prepareNewSector(uint8_t sector);
  void setBitInHeader(uint8_t sector, uint8_t entry);
  size_t getAddress(uint8_t sector, uint8_t entry) const;
  void incrementEntry(uint8_t &sectorToIncremenet,        // NOLINT
                      uint8_t &entryToIncremenet) const;  // NOLINT
  void incrementEntry();
  bool enabled = false;
  uint8_t currentSector = 0;
  uint8_t sectorsCount = 0;
  uint8_t nextFreeEntry = 0;
  const esp_partition_t *partition = nullptr;
  Supla::Html::SecurityLogList *htmlLog = nullptr;

  uint8_t sectorForOutput = 0;
  uint8_t entryNumberForOutput = 0;
  Supla::SecurityLogEntry entryForOutput;
};

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECURITY_LOGGER_H_
