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

/**
 * One firmware update attempt. SuplaDevice owns the updater, calls start()
 * and then calls iterate() until the attempt ends. The updater implements
 * version discovery, download, verification and installation for its platform.
 *
 * To use your own updater, derive from SwUpdate and register a factory before
 * SuplaDevice.begin(). This example assumes GitHubSwUpdate is defined by the
 * application and its constructor forwards the arguments to SwUpdate.
 *
 * @code{.cpp}
 * // In setup(), after configuring the device and before begin().
 * SuplaDevice.setSwUpdateFactory(
 *     [](SuplaDeviceClass *device,
 *        const char *url,
 *        Supla::SwUpdateMode mode) -> Supla::Device::SwUpdate * {
 *       return new GitHubSwUpdate(device, url, mode);
 *     });
 * SuplaDevice.setAutomaticFirmwareUpdateSupported(true);
 * @endcode
 *
 * The factory receives the configured update server URL or the SUPLA default.
 * SwUpdate copies it into its own buffer. The factory may supply a different
 * URL to the constructor, such as the project's GitHub Releases endpoint.
 * The factory's URL argument is valid only during the call. Return a new
 * object for each attempt, or nullptr to decline that attempt. Register a
 * nullptr factory to restore the platform default.
 *
 * Read mode and policy flags in iterate(). SuplaDevice attaches the observer
 * and applies beta, securityOnly and skipCert after construction. OnlyCheck
 * checks for a new version without writing to flash. CheckAndUpdate and
 * PeriodicCheckAndUpdate may install an update. A retry creates another
 * object in CheckAndUpdate mode.
 *
 * Only CheckAndUpdate switches the device to SW update mode, which sets the
 * status and disconnects protocols before start(). In PeriodicCheckAndUpdate
 * the device stays connected and keeps running its normal loop, so an updater
 * that installs in this mode must not block and must cope with the network
 * being shared with the SUPLA connection.
 *
 * SuplaDevice sets the security-only flag for periodic OTA under the
 * SecurityOnly policy and for a security update requested from Cloud.
 * Call isSecurityOnly() when selecting a release. The updater determines which
 * releases qualify using its update source, for example a security flag in the
 * project's release manifest. If no newer security release is available,
 * use the up-to-date path below. On retry, the security-only flag is carried
 * over from the previous attempt.
 *
 * When OnlyCheck finds an update, set newVersion before ending the attempt so
 * SuplaDevice can report it to Cloud. For example, given a discovered version
 * string called version (include <string.h> for strlen and memcpy)
 *
 * @code{.cpp}
 * delete[] newVersion;
 * const size_t length = strlen(version) + 1;
 * newVersion = new char[length];
 * memcpy(newVersion, version, length);
 * abort = true;
 * notifyFinished(true);
 * @endcode
 *
 * If the installed firmware is already up to date, leave newVersion unset.
 *
 * @code{.cpp}
 * abort = true;
 * notifyFinished(Supla::Device::SwUpdateResult::UP_TO_DATE);
 * @endcode
 *
 * To install an update, use the platform OTA API to write the image, verify
 * it and select it for the next boot. Only then report installation success.
 *
 * @code{.cpp}
 * finished = true;
 * notifyFinished(Supla::Device::SwUpdateResult::UPDATED);
 * @endcode
 *
 * For a failed attempt, set abort and report FAILED (or notifyFinished(false)).
 * Set retryAllowed for a transient failure when installing. SuplaDevice checks
 * abort first, releases the updater on either terminal state and schedules a
 * restart on finished. An OnlyCheck attempt is never retried, so a failed
 * check cannot turn into an installation.
 *
 * A reported failure takes precedence over newVersion in the Cloud result.
 * Otherwise, newVersion means an update is available, and retryAllowed
 * without a version means an error. With neither flag nor a new version,
 * ending the attempt with abort reports that no update is available.
 *
 * The updater may be deleted from SuplaDevice's destructor, which for the
 * global SuplaDevice runs during static destruction at program exit. Keep the
 * destructor simple: do not rely on other global objects or join threads that
 * depend on them.
 */
class SwUpdate {
 public:
  /** Creates the default platform updater, if available. */
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

  /**
   * Advances the attempt. Return promptly while waiting for asynchronous IO.
   */
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
  /** Returns whether the attempt reported a failure via notifyFinished(). */
  bool isFailed() const {
    return failed;
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

  /** Returns whether this attempt may only install security updates. */
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
    failed = !success;
    if (observer) {
      observer->onSwUpdateFinished(success, reason);
    }
  }
  void notifyFinished(SwUpdateResult result, const char *reason = nullptr) {
    failed = result == SwUpdateResult::FAILED;
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
  bool failed = false;
  SuplaDeviceClass *sdc = nullptr;
  SwUpdateObserver *observer = nullptr;

  // Optional strings owned by SwUpdate. Allocate with new[].
  // Set newVersion to report an available update back to Cloud.
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
