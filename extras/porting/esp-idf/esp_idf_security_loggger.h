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

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECURITY_LOGGGER_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECURITY_LOGGGER_H_

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

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_SECURITY_LOGGGER_H_
