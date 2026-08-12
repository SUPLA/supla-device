// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_CHANNEL_CONFLICT_RESOLVER_H_
#define SRC_SUPLA_DEVICE_CHANNEL_CONFLICT_RESOLVER_H_

#include <stdint.h>

namespace Supla {
namespace Device {

class ChannelConflictObserver {
 public:
  virtual ~ChannelConflictObserver() = default;
  virtual void onChannelConflictResolution(bool handled) = 0;
};

class ChannelConflictResolver {
 public:
  virtual ~ChannelConflictResolver() = default;
  virtual bool onChannelConflictReport(
      uint8_t *channelReport,
      uint8_t channelReportSize,
      bool hasConfilictInvalidType,
      bool hasConfilictChannelMissingOnServer,
      bool hasConflictChannelMissingOnDevice) = 0;

  void setChannelConflictObserver(ChannelConflictObserver *newObserver) {
    observer = newObserver;
  }

 protected:
  void notifyChannelConflictResolution(bool handled) {
    if (observer != nullptr) {
      observer->onChannelConflictResolution(handled);
    }
  }

 private:
  ChannelConflictObserver *observer = nullptr;
};

struct ChannelConflictResolverListItem {
  ChannelConflictResolver *resolver = nullptr;
  ChannelConflictResolverListItem *next = nullptr;
};

class ChannelConflictResolverList : public ChannelConflictResolver {
 public:
  ~ChannelConflictResolverList() override;

  bool add(ChannelConflictResolver *resolver);
  bool remove(ChannelConflictResolver *resolver);
  void clear();
  bool contains(ChannelConflictResolver *resolver) const;
  bool isEmpty() const;

  bool onChannelConflictReport(
      uint8_t *channelReport,
      uint8_t channelReportSize,
      bool hasConfilictInvalidType,
      bool hasConfilictChannelMissingOnServer,
      bool hasConflictChannelMissingOnDevice) override;

 private:
  ChannelConflictResolverListItem *first = nullptr;
};

}  // namespace Device
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_CHANNEL_CONFLICT_RESOLVER_H_
