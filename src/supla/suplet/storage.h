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

#ifndef SRC_SUPLA_SUPLET_STORAGE_H_
#define SRC_SUPLA_SUPLET_STORAGE_H_

#include <stdint.h>
#include <stddef.h>
#include <supla/suplet/channel_allocator.h>
#include <supla/suplet/config.h>

namespace Supla {

class Config;

namespace Suplet {

enum class InstanceState : uint8_t {
  Disabled = 0,
  Active = 1,
  Staged = 2,
  DeletePending = 3,
};

struct InstanceRecord {
  InstanceRecord();
  InstanceRecord(const InstanceRecord &other);
  InstanceRecord &operator=(const InstanceRecord &other);
  ~InstanceRecord();

  bool setConfig(const uint8_t *data, uint16_t size);
  void clearConfig();

  uint8_t instanceId = 0;
  uint32_t definitionId = 0;
  uint16_t definitionVersion = 0;
  uint8_t subDeviceId = 0;
  InstanceState state = InstanceState::Disabled;
  uint16_t configSize = 0;
  ChannelMap channelMap = {};
  uint8_t *config = nullptr;
};

class InstanceTable {
 public:
  InstanceTable();
  InstanceTable(const InstanceTable &) = delete;
  InstanceTable &operator=(const InstanceTable &) = delete;
  ~InstanceTable();

  uint8_t getCount() const;
  void clear();

  bool add(const InstanceRecord &record);
  bool removeByInstanceId(uint8_t instanceId);
  InstanceRecord *findByInstanceId(uint8_t instanceId);
  const InstanceRecord *findByInstanceId(uint8_t instanceId) const;
  InstanceRecord *findBySubDeviceId(uint8_t subDeviceId);
  const InstanceRecord *findBySubDeviceId(uint8_t subDeviceId) const;
  InstanceRecord *getRecord(uint8_t index);
  const InstanceRecord *getRecord(uint8_t index) const;

 private:
  bool resize(uint8_t newCount);

  InstanceRecord *records = nullptr;
  uint8_t count = 0;
};

class Storage {
 public:
  explicit Storage(Supla::Config *config);

  bool load(InstanceTable *table);
  bool loadIndex(InstanceTable *table);
  bool loadInstance(uint8_t instanceId, InstanceRecord *record);
  bool save(const InstanceTable &table);
  bool erase();

 private:
#pragma pack(push, 1)
  struct StoredInstanceHeader {
    uint8_t version = 0;
    uint8_t state = 0;
    uint32_t definitionId = 0;
    uint16_t definitionVersion = 0;
    uint8_t channelCount = 0;
    uint16_t configSize = 0;
  };

  struct StoredChannelMapping {
    uint8_t channelId = 0;
    uint8_t channelNumber = 0;
  };
#pragma pack(pop)

  bool loadVariant(uint8_t instanceId, uint8_t variant, InstanceRecord *record)
      const;
  bool loadVariant(uint8_t instanceId,
                   uint8_t variant,
                   InstanceRecord *record,
                   bool loadConfig) const;
  bool loadActiveVariant(uint8_t instanceId,
                         uint8_t *activeVariant,
                         InstanceRecord *record,
                         bool loadConfig);
  bool saveVariant(const InstanceRecord &record, uint8_t variant);
  bool eraseVariant(uint8_t instanceId, uint8_t variant);
  bool eraseInstance(uint8_t instanceId);
  bool cleanupLoadedInstance(uint8_t instanceId,
                             uint8_t activeVariant,
                             uint8_t loadedVariant);
  bool slotExists(uint8_t instanceId) const;
  void makeActKey(uint8_t instanceId, char *output) const;
  void makeHeaderKey(uint8_t instanceId, uint8_t variant, char *output) const;
  void makeChannelMapKey(uint8_t instanceId,
                         uint8_t variant,
                         char *output) const;
  void makeConfigKey(uint8_t instanceId, uint8_t variant, char *output) const;
  bool readBlobExact(const char *key, char *output, size_t expectedSize) const;

  Supla::Config *config = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_STORAGE_H_
