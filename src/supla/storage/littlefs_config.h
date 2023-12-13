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

#ifndef SRC_SUPLA_STORAGE_LITTLEFS_CONFIG_H_
#define SRC_SUPLA_STORAGE_LITTLEFS_CONFIG_H_

#ifndef SUPLA_EXCLUDE_LITTLEFS_CONFIG

#define SUPLA_LITTLEFS_CONFIG_BUF_SIZE 1024

#include "key_value.h"

namespace Supla {

class LittleFsConfig : public KeyValue {
 public:
  explicit LittleFsConfig(int configMaxSize = SUPLA_LITTLEFS_CONFIG_BUF_SIZE);
  virtual ~LittleFsConfig();
  bool init() override;
  void commit() override;
  void removeAll() override;

  bool getCustomCA(char* result, int maxSize) override;
  int getCustomCASize() override;
  bool setCustomCA(const char* customCA) override;

  // override blob storage to use separate file for each value
  bool setBlob(const char* key, const char* value, size_t blobSize) override;
  bool getBlob(const char* key, char* value, size_t blobSize) override;

 protected:
  int getBlobSize(const char* key) override;
  bool initLittleFs();
  int configMaxSize = SUPLA_LITTLEFS_CONFIG_BUF_SIZE;
};
};  // namespace Supla

#endif  // SUPLA_EXCLUDE_LITTLEFS_CONFIG
#endif  // SRC_SUPLA_STORAGE_LITTLEFS_CONFIG_H_
