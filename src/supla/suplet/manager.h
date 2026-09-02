// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SUPLET_MANAGER_H_
#define SRC_SUPLA_SUPLET_MANAGER_H_

#include <stdint.h>
#include <supla/device/channel_conflict_resolver.h>
#include <supla-common/proto.h>
#include <supla/suplet/definition.h>
#include <supla/suplet/registry.h>
#include <supla/suplet/storage.h>

class SuplaDeviceClass;

namespace Supla {

class Config;
class Element;

namespace Suplet {

class CapabilityRegistry;
struct DefinitionCalcfgSession;
struct InstanceCalcfgSession;
class ServerConfigHandler;
enum class ServerConfigResult : uint8_t;

class Manager : public Supla::Device::ChannelConflictResolver {
 public:
  explicit Manager(Supla::Config *config);
  ~Manager();

  bool load();
  bool loadInstance(uint8_t instanceId, InstanceRecord *record);
  bool save();
  bool erase();

  InstanceTable *getInstanceTable();
  const InstanceTable *getInstanceTable() const;

  void setRegistry(Registry *registry);
  Registry *getRegistry();
  const Registry *getRegistry() const;
  void setCapabilityRegistry(CapabilityRegistry *registry);
  CapabilityRegistry *getCapabilityRegistry();
  const CapabilityRegistry *getCapabilityRegistry() const;
  void setServerConfigHandler(ServerConfigHandler *handler);
  ServerConfigHandler *getServerConfigHandler();
  const ServerConfigHandler *getServerConfigHandler() const;
  bool isServerConfigReady() const;

  bool addInstance(const InstanceRecord &record);
  bool addInstanceFromDefinition(InstanceRecord record,
                                 const Definition &definition);
  bool canUpsertInstanceFromDefinition(InstanceRecord record,
                                       const Definition &definition) const;
  bool upsertInstanceFromDefinition(InstanceRecord record,
                                    const Definition &definition);
  bool createElementsFromRegistry(const Registry &registry,
                                  Supla::Element **created,
                                  uint16_t createdSize,
                                  uint16_t *createdCount = nullptr);
  bool loadRuntimeElements();
  bool loadRuntimeElementsFromRegistry(const Registry &registry);
  void deleteRuntimeElements();
  bool initRuntimeElements(SuplaDeviceClass *device);
  uint16_t getRuntimeElementCount() const;
  ServerConfigResult applyCommandJson(const char *commandJson);
  ServerConfigResult validateCommandJson(const char *commandJson) const;
  int handleCalcfg(TSD_DeviceCalCfgRequest *request,
                   TDS_DeviceCalCfgResult *result);
  InstanceCalcfgSession *getInstanceCalcfgSession();
  const InstanceCalcfgSession *getInstanceCalcfgSession() const;
  InstanceCalcfgSession *beginInstanceCalcfgSession();
  void clearInstanceCalcfgSession();
  DefinitionCalcfgSession *getDefinitionCalcfgSession();
  const DefinitionCalcfgSession *getDefinitionCalcfgSession() const;
  DefinitionCalcfgSession *beginDefinitionCalcfgSession();
  void clearDefinitionCalcfgSession();
  void cleanupExpiredCalcfgSessions(uint32_t nowMs);
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
  bool getRequiredRuntimeElementCount(const Registry &registry,
                                      uint16_t *count) const;

  Supla::Suplet::Storage storage;
  InstanceTable table;
  Registry *registry = nullptr;
  CapabilityRegistry *capabilityRegistry = nullptr;
  ServerConfigHandler *serverConfigHandler = nullptr;
  Supla::Element **runtimeElements = nullptr;
  uint16_t runtimeElementCount = 0;
  InstanceCalcfgSession *instanceCalcfgSession = nullptr;
  DefinitionCalcfgSession *definitionCalcfgSession = nullptr;
};

}  // namespace Suplet
}  // namespace Supla

#endif  // SRC_SUPLA_SUPLET_MANAGER_H_
