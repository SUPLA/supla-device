// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "sleepy_thermometer.h"

#include <SuplaDevice.h>
#include <supla/channels/channel.h>
#include <supla/protocol/supla_srpc.h>
#include <time.h>

#include <cinttypes>
#include <cstdio>

#include "suplan_runtime_linux.h"

namespace {

uint64_t bootMs() {
  timespec ts = {};
  clock_gettime(CLOCK_BOOTTIME, &ts);
  return static_cast<uint64_t>(ts.tv_sec) * 1000 + ts.tv_nsec / 1000000;
}

}  // namespace

namespace Supla {

SleepyThermometer::SleepyThermometer(
    uint64_t cycle, uint32_t readyMs, uint16_t awakeMs, uint32_t tailMs,
    uint64_t nextStartMs)
    : cycle_(cycle),
      startMs_(bootMs()),
      nextStartMs_(nextStartMs),
      readyMs_(readyMs),
      tailMs_(tailMs),
      awakeMs_(awakeMs) {
  log("START");
}

void SleepyThermometer::log(const char* event, const char* details) const {
  timespec ts = {};
  clock_gettime(CLOCK_REALTIME, &ts);
  const uint64_t now = bootMs();
  printf("SLEEPY cycle=%" PRIu64 " ts=%" PRId64 ".%03ld boot_ms=%" PRIu64
         " elapsed_ms=%" PRIu64
         " event=%s %s\n",
         cycle_, static_cast<int64_t>(ts.tv_sec), ts.tv_nsec / 1000000,
         now, now - startMs_, event, details);
  fflush(stdout);
}

void SleepyThermometer::progress(const char* event, uint32_t count,
                                 uint32_t& seen) const {
  if (count == seen) return;
  char detail[48];
  snprintf(detail, sizeof(detail), "count=%" PRIu32, count);
  log(event, detail);
  seen = count;
}

bool SleepyThermometer::initialize() {
  auto* channel = Channel::Begin();
  if (!channel || channel->next() ||
      channel->getChannelType() != SUPLA_CHANNELTYPE_THERMOMETER) {
    failed_ = true;
    log("SYNC_FAILED", "reason=expected_one_thermometer");
    return false;
  }
  const double temperature = 20.0 + ((cycle_ - 1) % 4) * 0.5;
  if (channel->getValueDouble() != temperature) {
    failed_ = true;
    log("SYNC_FAILED", "reason=thermometer_value_does_not_match_cycle");
    return false;
  }
  char detail[48];
  snprintf(detail, sizeof(detail), "celsius=%.1f", temperature);
  log("TEMPERATURE_UPDATED", detail);
  channelNumber_ = channel->getChannelNumber();
  return true;
}

bool SleepyThermometer::iterate(LinuxSupLanRuntime& runtime) {
  auto* protocol = runtime.protocol();
  const auto& diagnostics = runtime.diagnostics();
  const uint64_t now = bootMs();
  const bool udpOpen = runtime.isTransportOpen();
  if (!udpSeen_ && udpOpen) {
    log("UDP_READY");
    udpSeen_ = true;
  }
  const auto* srpc = SuplaDevice.getSrpcLayer();
  if (!serverSeen_ && srpc && srpc->serverIdentity().serverSyncComplete()) {
    log("SERVER_READY", "source=live_sync");
    serverSeen_ = true;
  }
  progress("INTEREST_RESET", runtime.resetsSent(), resets_);
  progress("PEER_AWAKE", runtime.awakeHintsSent(), awakeHints_);
  progress("READ_RECEIVED", diagnostics.readDispatched, reads_);
  progress("RETRY", diagnostics.retryTx, retries_);

  if (!wakeMs_) {
    const char* blocker = runtime.sleepySourceBlocker(channelNumber_);
    if (now - startMs_ >= readyMs_) {
      const char* reason = !udpOpen ? "udp_not_ready" :
          (blocker ? blocker : "readiness_deadline");
      char detail[64];
      snprintf(detail, sizeof(detail), "reason=%s", reason);
      log("SYNC_FAILED", detail);
      failed_ = true;
      return false;
    }
    if (udpOpen && !blocker) {
      if (!serverSeen_) {
        log("SERVER_READY", "source=durable_identity_and_grant");
        serverSeen_ = true;
      }
      protocol->beginWake(awakeMs_, false);
      wakeMs_ = now;
      stateBaseline_ = diagnostics.stateNotificationTx;
      log("BEGIN_WAKE", "retained_interests=false");
    }
    return true;
  }

  progress("STATE_SENT", diagnostics.stateNotificationTx - stateBaseline_,
           states_);
  if (sleepMs_) {
    if (now - sleepMs_ < tailMs_) return true;
    const bool sent = runtime.sleepAnnouncementSent();
    log("SLEEP_RESULT", sent ? "sent=1" : "sent=0");
    if (!sent) {
      log("SYNC_FAILED", "reason=sleep_not_sent");
      failed_ = true;
    }
    return false;
  }

  // The normal completion predicate includes bounded peer fan-out. A
  // disappearing interface can stop that fan-out, so bound it here as well.
  if (!protocol->wakeWindowComplete()) {
    if (now - wakeMs_ < awakeMs_ + SUPLAN_MAX_PERSISTENT_PEERS * 10U + 1000U)
      return true;
    log("SYNC_FAILED", "reason=wake_window_incomplete");
    failed_ = true;
    return false;
  }
  if (!states_) {
    log("SYNC_FAILED", reads_ ? "reason=no_state_sent" : "reason=no_read");
    failed_ = true;
  }
  const uint64_t remaining = nextStartMs_ > now ? nextStartMs_ - now : 0;
  const uint32_t duration = static_cast<uint32_t>((remaining + 999) / 1000);
  char detail[96];
  const auto before = diagnostics.dataTx;
  if (udpOpen && duration) runtime.announceSleep(duration);
  snprintf(detail, sizeof(detail),
           "duration_sec=%" PRIu32 " initial_datagrams=%" PRIu32,
           duration, diagnostics.dataTx - before);
  log("ANNOUNCE_SLEEP", detail);
  sleepMs_ = now;
  return true;
}

int SleepyThermometer::finish(bool saved, bool stopped) const {
  const int status = !saved ? 1 : failed_ ? 3 : 0;
  char detail[48];
  snprintf(detail, sizeof(detail), "code=%d stopped=%u", status, stopped);
  log("EXIT", detail);
  return status;
}

}  // namespace Supla
