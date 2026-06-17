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

#ifndef SRC_SUPLA_SUPLET_MANAGER_H_
#define SRC_SUPLA_SUPLET_MANAGER_H_

#include <stdint.h>
#include <supla/device/channel_conflict_resolver.h>
#include <supla/suplet/definition.h>
#include <supla/suplet/registry.h>
#include <supla/suplet/storage.h>

namespace Supla {

class Config;
class Element;

namespace Suplet {

class Manager : public Supla::Device::ChannelConflictResolver {
 public:
  explicit Manager(Supla::Config *config);

  bool load();
  bool save();
  bool erase();

  InstanceTable *getInstanceTable();
  const InstanceTable *getInstanceTable() const;

  bool addInstance(const InstanceRecord &record);
  bool addInstanceWithAllocatedChannels(InstanceRecord record,
                                        const uint32_t *requiredChannelKeys,
                                        uint8_t requiredChannelKeyCount,
                                        const ChannelAllocator &occupied);
  bool addInstanceFromDefinition(InstanceRecord record,
                                 const Definition &definition,
                                 const ChannelAllocator &occupied);
  bool canUpsertInstanceFromDefinition(InstanceRecord record,
                                       const Definition &definition,
                                       const ChannelAllocator &occupied) const;
  bool upsertInstanceFromDefinition(InstanceRecord record,
                                    const Definition &definition,
                                    const ChannelAllocator &occupied);
  bool createElementsFromRegistry(const Registry &registry,
                                  Supla::Element **created,
                                  uint16_t createdSize,
                                  uint16_t *createdCount = nullptr);
  bool removeInstance(uint8_t instanceId);
  uint8_t getFirstFreeSubDeviceId() const;

  bool onChannelConflictReport(uint8_t *channelReport,
                               uint8_t channelReportSize,
                               bool hasConfilictInvalidType,
                               bool hasConfilictChannelMissingOnServer,
                               bool hasConflictChannelMissingOnDevice) override;

 private:
  bool isChannelMissingOnServer(uint8_t *channelReport,
                                uint8_t channelReportSize,
                                int channelNumber) const;
  bool markExistingSupletChannels(ChannelAllocator *allocator) const;
  bool markExistingSupletChannelsExcept(ChannelAllocator *allocator,
                                        uint8_t instanceId) const;

  Supla::Suplet::Storage storage;
  InstanceTable table;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_MANAGER_H_
