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

#ifndef EXTRAS_PORTING_ESP_IDF_NVS_CONFIG_H_
#define EXTRAS_PORTING_ESP_IDF_NVS_CONFIG_H_

#include <nvs.h>
#include <supla/storage/config.h>

namespace Supla {

class NvsConfig : public Config {
 public:
  NvsConfig();
  virtual ~NvsConfig();
  bool init() override;
  void removeAll() override;

  bool generateGuidAndAuthkey() override;

  // Generic getters and setters
  bool setString(const char* key, const char* value) override;
  bool getString(const char* key, char* value, size_t maxSize) override;
  int getStringSize(const char* key) override;

  bool setBlob(const char* key, const char* value, size_t blobSize) override;
  bool getBlob(const char* key, char* value, size_t blobSize) override;

  bool getInt8(const char* key, int8_t* result) override;
  bool getUInt8(const char* key, uint8_t* result) override;
  bool getInt32(const char* key, int32_t* result) override;
  bool getUInt32(const char* key, uint32_t* result) override;

  bool setInt8(const char* key, const int8_t value) override;
  bool setUInt8(const char* key, const uint8_t value) override;
  bool setInt32(const char* key, const int32_t value) override;
  bool setUInt32(const char* key, const uint32_t value) override;
  bool eraseKey(const char* key) override;

  void commit() override;

 protected:
  int getBlobSize(const char* key) override;
  nvs_handle_t nvsHandle = 0;
};
};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_NVS_CONFIG_H_
