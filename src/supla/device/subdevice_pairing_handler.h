// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_SUBDEVICE_PAIRING_HANDLER_H_
#define SRC_SUPLA_DEVICE_SUBDEVICE_PAIRING_HANDLER_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {
namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol

namespace Device {

class SubdevicePairingObserver {
 public:
  virtual ~SubdevicePairingObserver() = default;

  virtual void onSubdevicePairingStarted(uint16_t maximumDurationSec) = 0;
  virtual void onSubdevicePairingFinished(
      const TCalCfg_SubdevicePairingResult &result) = 0;
};

class SubdevicePairingHandler {
 public:
  virtual ~SubdevicePairingHandler() = default;

  virtual bool startPairing(Supla::Protocol::SuplaSrpc *srpc,
                            TCalCfg_SubdevicePairingResult *result) = 0;

  void setPairingObserver(SubdevicePairingObserver *newObserver) {
    observer = newObserver;
  }

 protected:
  void notifySubdevicePairingStarted(uint16_t maximumDurationSec) {
    if (observer != nullptr) {
      observer->onSubdevicePairingStarted(maximumDurationSec);
    }
  }

  void notifySubdevicePairingFinished(
      const TCalCfg_SubdevicePairingResult &result) {
    if (observer != nullptr) {
      observer->onSubdevicePairingFinished(result);
    }
  }

 private:
  SubdevicePairingObserver *observer = nullptr;
};

}  // namespace Device
}  // namespace Supla


#endif  // SRC_SUPLA_DEVICE_SUBDEVICE_PAIRING_HANDLER_H_
