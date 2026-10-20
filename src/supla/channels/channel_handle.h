// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_CHANNELS_CHANNEL_HANDLE_H_
#define SRC_SUPLA_CHANNELS_CHANNEL_HANDLE_H_
#ifndef ARDUINO_ARCH_AVR
#include <stddef.h>
#include <supla/action_handler.h>
#include <supla/channels/channel_reference.h>
#include <supla/channels/channel_state.h>
#include <supla/channels/remote_channel_manager.h>
#include <supla/local_action.h>
#include <suplan/suplan_config.h>

namespace Supla {
class Condition;
enum ChannelPermission : uint8_t {
  READ = SupLan::kPermissionRead,
  CONTROL = SupLan::kPermissionControl,
  ACTION = SupLan::kPermissionAction
};
// Non-owning, location-neutral facade. The application owns its lifetime.
// Construct/delete/configure in ordinary loop context, never from an ISR.
// deactivate() is also safe inside an action callback; delete later in loop.
// Borrowed state must not outlive the manager/cache or be retained across loop.
// bool commands acknowledge local admission only. Avoid cyclic action bindings;
// protected retry/dedupe bounds one command, not application feedback loops.
class ChannelHandle : public LocalAction, public ActionHandler,
                      public ChannelStateProvider {
 public:
  ChannelHandle(uint32_t channelId, uint8_t permissions,
                RemoteDelivery delivery = RemoteDelivery::SupLanPeer);
  ~ChannelHandle() override;
  ChannelHandle(const ChannelHandle &) = delete;
  ChannelHandle &operator=(const ChannelHandle &) = delete;
  bool configure(uint32_t channelId, uint8_t permissions);
  void deactivate();
  ChannelState state() override;
  ChannelState channelState() { return state(); }
  Channel *localChannel() override;
  bool canRead();
  bool canControl();
  ChannelAccessStatus accessStatus();
  bool setValue(const uint8_t value[8], uint32_t durationMs = 0);
  bool executeAction(uint32_t actionId, const uint8_t *params = nullptr,
                     size_t length = 0);
  using LocalAction::addAction;
  void addAction(uint16_t action, ActionHandler &client, uint16_t event,
                 Condition *condition, bool alwaysEnabled = false);
  void addAction(uint16_t action, ActionHandler *client, uint16_t event,
                 Condition *condition, bool alwaysEnabled = false);
  void handleAction(int event, int action) override;
  uint32_t channelId() const { return id_; }
  RemoteDelivery delivery() const { return delivery_; }
  static void iterateAll();
  static void invalidateServerScope();
  static void receiveAction(uint32_t channelId, uint32_t capability);

 protected:
  bool remoteOnly_ = false;

 private:
  void reconcile();
  void observe();
  void baselineConditions();
  Device::RemoteChannelManager *manager() const;
  RemoteChannelKey key() const { return {id_, delivery_}; }
  static ChannelHandle *after(uint32_t token, uint32_t limit);
  static ChannelHandle *first_;
  static uint32_t nextToken_;
  ChannelHandle *next_ = nullptr;
  uint32_t token_ = 0;
  uint32_t id_ = 0;
  uint32_t type_ = 0;
  uint32_t function_ = 0;
  uint32_t baselineExpires_ = 0;
  uint32_t continuity_ = 0;
  uint8_t baseline_[8] = {};
  uint8_t permissions_ = 0;
  uint8_t localNumber_ = 0xff;
  ChannelResolutionKind location_ : 2;
  uint8_t active_ : 1;
  uint8_t admitted_ : 1;
  uint8_t baselineValid_ : 1;
  uint8_t baselineTimed_ : 1;
  RemoteDelivery delivery_ : 2;
};
class ServerRemoteChannel : public ChannelHandle {
 public:
  explicit ServerRemoteChannel(uint32_t id)
      : ChannelHandle(id, READ, RemoteDelivery::ServerStream) {}
};
class RemoteChannel : public ChannelHandle {
 public:
  RemoteChannel(uint32_t channelId, uint8_t permissions)
      : ChannelHandle(channelId, permissions) { remoteOnly_ = true; }
};
static_assert(sizeof(ChannelHandle) <= (sizeof(void *) == 4 ? 60 : 80),
              "ChannelHandle RAM budget exceeded");
static_assert(sizeof(RemoteChannel) == sizeof(ChannelHandle),
              "RemoteChannel must not own a second state cache");
}  // namespace Supla
#endif  // !ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_CHANNELS_CHANNEL_HANDLE_H_
