// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_SERVER_IDENTITY_H_
#define SRC_SUPLA_DEVICE_SERVER_IDENTITY_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {
class Config;
namespace Device {

enum class ServerChannelLocation : uint8_t { kLocal, kRemote, kUnresolved };

struct ServerChannelResolution {
  ServerChannelLocation location;
  uint8_t channelNumber;
};

// Production SERVER state, independent of PoC mappings and peer/session state.
// ChannelIds are positional in registration order, never ChannelNumbers.
class ServerIdentity {
 public:
  void load(Config *config);
  bool registrationStarted();
  void registrationSucceeded();
  void disconnected();
  void syncDone();
  TDS_SuplaDeviceIdentitiesResult accept(
      const TSD_SuplaDeviceIdentities &snapshot);

  ServerChannelResolution resolve(uint32_t channelId) const;
  bool reverse(uint8_t channelNumber, uint32_t *channelId) const;
  // Configuration must durably forget an association before reusing its number.
  bool forgetChannel(uint8_t channelNumber);
  bool registrationContextValid() const;
  bool registrationInvalidated() const;
  bool identityAvailable() const;
  bool identityTransition() const;
  bool serverSyncComplete() const;
  uint32_t rootEpoch() const { return rootValid ? epoch : 0; }
  const uint8_t *rootKey() const { return rootValid ? root : nullptr; }
  int32_t serverDeviceId() const { return identityAvailable() ? deviceId : 0; }

 private:
  static const int kIdentityPrefix = 6;
  static const int kIdentityMaxSize =
      kIdentityPrefix + 5 * SUPLA_CHANNELMAXCOUNT + 2;
  bool ensureRoot();
  bool persist(const char *key, const uint8_t *data, int size);

  Config *config = nullptr;
  uint8_t root[32] = {};
  uint32_t epoch = 0;
  int32_t deviceId = 0;
  uint32_t registrationGeneration = 0;
  uint32_t identityGeneration = 0;
  uint8_t registrationCount = 0;
  bool rootValid = false;
  bool identityValid = false;
  bool rootDurable = false;
  bool identityDurable = false;
  bool registrationPrepared = false;
  bool registrationValid = false;
  bool transition = false;
  bool syncComplete = false;
};

}  // namespace Device
}  // namespace Supla
#endif  // SRC_SUPLA_DEVICE_SERVER_IDENTITY_H_
