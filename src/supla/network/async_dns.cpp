// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "async_dns.h"
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32) || \
    defined(ESP_PLATFORM)
#ifndef ARDUINO_ARCH_ESP8266
#include <atomic>
#endif
#include <string.h>
#include <lwip/dns.h>
#include <lwip/tcpip.h>

namespace {
enum { Free, Pending, Done, Abandoned };
#ifdef ARDUINO_ARCH_ESP8266
// ESP8266 lwIP/DNS callbacks run on the cooperative main context, not an ISR.
// Xtensa LX106 has no lock-free exchange instruction/libatomic implementation.
struct JobState {
  int value = Free;
  int load() const { return value; }
  void store(int next) { value = next; }
  int exchange(int next) { int old = value; value = next; return old; }
  bool compare_exchange_strong(int &expected, int next) {
    if (value != expected) {
      expected = value;
      return false;
    }
    value = next;
    return true;
  }
};
#else
using JobState = std::atomic<int>;
#endif
struct DnsSlot {
  JobState state{Free};
  char host[256] = {};
  uint32_t address = 0;
};
DnsSlot jobs[4];
void resolved(const char *, const ip_addr_t *address, void *context) {
  auto job = static_cast<DnsSlot *>(context);
  job->address = address && IP_IS_V4(address)
      ? ip4_addr_get_u32(ip_2_ip4(address)) : 0;
  if (job->state.exchange(Done) == Abandoned) job->state.store(Free);
}
void submit(void *context) {
  auto job = static_cast<DnsSlot *>(context);
  if (job->state.load() == Abandoned) {
    job->state.store(Free);
    return;
  }
  ip_addr_t address;
  err_t error = dns_gethostbyname(job->host, &address, resolved, job);
  if (error != ERR_INPROGRESS)
    resolved(nullptr, error == ERR_OK ? &address : nullptr, job);
}
}  // namespace

bool Supla::AsyncDns::start(const char *host) {
  cancel();
  if (!host || strlen(host) >= sizeof(jobs[0].host)) return false;
  for (int i = 0; i < 4; ++i) {
    int expected = Free;
    if (!jobs[i].state.compare_exchange_strong(expected, Pending)) continue;
    memcpy(jobs[i].host, host, strlen(host) + 1);
    jobs[i].address = 0;
    slot_ = i;
#ifdef ARDUINO_ARCH_ESP8266
    submit(&jobs[i]);
#else
    // DNS must run in lwIP's core context, without waiting for its mailbox.
    if (tcpip_try_callback(submit, &jobs[i]) != ERR_OK) {
      jobs[i].state.store(Free);
      slot_ = -1;
      return false;
    }
#endif
    return true;
  }
  return false;
}

int Supla::AsyncDns::poll(uint32_t *address) {
  if (slot_ < 0) return -1;
  auto &job = jobs[slot_];
  if (job.state.load() != Done) return 0;
  *address = job.address;
  job.state.store(Free);
  slot_ = -1;
  return *address ? 1 : -1;
}

void Supla::AsyncDns::cancel() {
  if (slot_ < 0) return;
  auto &job = jobs[slot_];
  if (job.state.exchange(Abandoned) == Done) job.state.store(Free);
  slot_ = -1;
}
#endif
