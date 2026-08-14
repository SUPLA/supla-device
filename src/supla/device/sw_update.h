// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_SW_UPDATE_H_
#define SRC_SUPLA_DEVICE_SW_UPDATE_H_

#define SUPLA_MAX_URL_LENGTH 202

#include <SuplaDevice.h>

namespace Supla {

namespace Device {
enum class SwUpdateResult : uint8_t {
  UPDATED,
  UP_TO_DATE,
  FAILED,
};

class SwUpdateObserver {
 public:
  virtual ~SwUpdateObserver() = default;
  virtual void onSwUpdateStarted() = 0;
  virtual void onSwUpdateProgress(uint32_t downloadedBytes,
                                  uint32_t totalBytes) = 0;
  virtual void onSwUpdateFinished(bool success, const char *reason) = 0;
  virtual void onSwUpdateResult(SwUpdateResult result, const char *reason) {
    onSwUpdateFinished(result != SwUpdateResult::FAILED, reason);
  }
};

class SwUpdate {
 public:
  static SwUpdate *Create(SuplaDeviceClass *sdc,
                          const char *newUrl,
                          Supla::SwUpdateMode mode);
  virtual ~SwUpdate();

  void start() {
    started = true;
    if (observer) {
      observer->onSwUpdateStarted();
    }
  }
  void setObserver(SwUpdateObserver *newObserver) { observer = newObserver; }
  virtual void iterate() = 0;

  void setUrl(const char *newUrl);
  bool isStarted() {
    return started;
  }
  bool isFinished() {
    return finished;
  }
  bool isAborted() {
    return abort;
  }
  void useBeta() {
    beta = true;
  }
  void setSkipCert() {
    // One-time recovery fallback for expired OTA certificates.
    // The OTA flow clears this mode after use.
    skipCert = true;
  }
  bool isRetryAllowed() {
    return retryAllowed;
  }

  const char *getUrl() const {
    return updateUrl;
  }
  const char *getNewVersion() const {
    return newVersion;
  }
  const char *getChangelogUrl() const {
    return changelogUrl;
  }

  bool isSecurityOnly() const {
    return securityOnly;
  }
  void setSecurityOnly() {
    securityOnly = true;
  }

 protected:
  explicit SwUpdate(SuplaDeviceClass *sdc,
                    const char *newUrl,
                    Supla::SwUpdateMode mode);

  void notifyProgress(uint32_t downloadedBytes, uint32_t totalBytes) {
    if (observer) {
      observer->onSwUpdateProgress(downloadedBytes, totalBytes);
    }
  }
  void notifyFinished(bool success, const char *reason = nullptr) {
    if (observer) {
      observer->onSwUpdateFinished(success, reason);
    }
  }
  void notifyFinished(SwUpdateResult result, const char *reason = nullptr) {
    if (observer) {
      observer->onSwUpdateResult(result, reason);
    }
  }

  bool beta = false;
  bool skipCert = false;
  bool securityOnly = false;
  bool started = false;
  bool finished = false;
  bool abort = false;
  SuplaDeviceClass *sdc = nullptr;
  SwUpdateObserver *observer = nullptr;
  char *updateUrl = nullptr;
  char *newVersion = nullptr;
  char *changelogUrl = nullptr;
  bool retryAllowed = false;
  Supla::SwUpdateMode mode = Supla::SwUpdateMode::NotSet;

  char url[SUPLA_MAX_URL_LENGTH] = {};
};
};  // namespace Device
};  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_SW_UPDATE_H_
