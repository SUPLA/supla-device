// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef SRC_SUPLA_SUPLAN_REMOTE_ACCESS_PORT_H_
#define SRC_SUPLA_SUPLAN_REMOTE_ACCESS_PORT_H_
#include <supla-common/proto.h>
namespace Supla {
namespace Device {
class RemoteChannelManager;
class RemoteAccessPort {
 public:
  virtual ~RemoteAccessPort() = default;
  virtual bool prepareBackend(uint8_t, RemoteChannelManager *) {
    return false;
  }
  virtual bool ensure(const TDS_SuplaEnsureResourceAccess &) { return false; }
  // ENSURE_RESULT has no resource/correlation ID. Cancellation must drain or
  // quarantine the old wire reply before another request can be admitted.
  // SuplaSrpc preserves its pending-response tombstone until reply/disconnect.
  virtual void cancelEnsure() {}
  virtual bool share(const TDS_SuplaEnsureResourceShare &) { return false; }
  virtual void cancelShare() {}
};
}  // namespace Device
}  // namespace Supla
#endif  // SRC_SUPLA_SUPLAN_REMOTE_ACCESS_PORT_H_
