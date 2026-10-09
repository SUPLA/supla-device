// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_RESOURCE_BINDING_MANAGER_H_
#define SRC_SUPLA_SUPLAN_RESOURCE_BINDING_MANAGER_H_
#ifndef ARDUINO_ARCH_AVR
#include <suplan/suplan_runtime.h>
#include <supla/suplan/remote_access_port.h>

namespace Supla {
namespace Device {
class ServerAssociations;
class RemoteResourceManager;
// Destination topology is durable; Source routing is derived volatile state.
class ResourceBindingManager {
 public:
  ResourceBindingManager(SupLan::PeerTable *peers,
                         SupLan::Runtime *runtime,
                         RemoteResourceManager *resources,
                         RemoteAccessPort *port);
  ~ResourceBindingManager();
  void attachRuntime(SupLan::Runtime *runtime);
  void attachAssociations(ServerAssociations *associations);
  void iterate(uint32_t now);
  void ensureShareResult(const TSD_SuplaEnsureResourceShareResult &result);
  uint8_t accept(uint8_t peer, const uint8_t *body, size_t size);
  bool authorizationReplacing(const SupLan::PeerContext &context,
      const SupLan::AclEntry *expected, uint16_t count);
  bool relayDemand(uint8_t target, bool *configured);
  void acknowledged(uint8_t peer, uint8_t result);
  void serverReconnected();
  int bindingCount() const;
  bool sessionNeeded(uint8_t peer) const;
  SupLan::BindingRetry *retryStorage() { return &retry_; }

 private:
  static const int kRecordCapacity = 29 + 12 * SUPLAN_MAX_BINDINGS_PER_PEER;
  struct Set {
    uint8_t context[27] = {};
    bool used = false;
    bool uncertain = false;
  } sets_[SUPLAN_MAX_PERSISTENT_PEERS];
  struct Contributor {
    uint32_t source = 0;
    uint32_t target = 0;
    uint32_t freshMs = 0;
    uint8_t set = 0xff;
    bool active = false;
  } contributors_[SUPLAN_MAX_RESOURCE_BINDINGS];
  struct Intent {
    uint32_t source = 0;
    uint32_t target = 0;
    uint32_t destination = 0;
    uint32_t nextEnsure = 0;
    bool retained = false;
    bool attempted = false;
    bool blocked = false;
  } intents_[SUPLAN_MAX_RESOURCE_BINDINGS];
  struct Sent {
    uint32_t hash = 0;
    uint32_t time = 0;
    bool attempted = false;
  } sent_[SUPLAN_MAX_PERSISTENT_PEERS];
  SupLan::BindingRetry retry_;
  SupLan::PeerTable *peers_;
  SupLan::Runtime *runtime_;
  RemoteResourceManager *resources_;
  RemoteAccessPort *port_;
  ServerAssociations *associations_ = nullptr;
  uint8_t record_[kRecordCapacity] = {};
  uint8_t previous_[kRecordCapacity] = {};
  int pendingIntent_ = -1;
  uint32_t ensureDeadline_ = 0;
  bool loaded_ = false;
  bool intentOverflow_ = false;
  uint32_t cleanupNextMs_ = 0;
  void load();
  bool persist(uint8_t set, const uint8_t *record, size_t length);
  bool validateBody(uint8_t peer, const uint8_t *body, size_t size,
                    bool authorization = true) const;
  void activate(uint8_t set, const uint8_t *body, size_t size);
  void detach(uint8_t set);
  int peerForSet(uint8_t set) const;
  bool collectIntents();
  void addIntent(uint32_t source, uint32_t target);
  int targetNumber(uint32_t id) const;
  void sendDesired(uint32_t now);
};
// Per-peer set/routing storage, contributor/intent slots, protected retry,
// two bounded record workspaces and fixed metadata. Independent of ACL pools.
static_assert(sizeof(ResourceBindingManager) <=
                  41 * SUPLAN_MAX_PERSISTENT_PEERS +
                  36 * SUPLAN_MAX_RESOURCE_BINDINGS +
                  sizeof(SupLan::BindingRetry) +
                  2 * (29 + 12 * SUPLAN_MAX_BINDINGS_PER_PEER) + 64,
              "Resource bindings exceed their bounded workspace");
}  // namespace Device
}  // namespace Supla
#endif
#endif  // SRC_SUPLA_SUPLAN_RESOURCE_BINDING_MANAGER_H_
