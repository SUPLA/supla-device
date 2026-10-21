// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "async_dns.h"
#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32) || \
    defined(ESP_PLATFORM)
#ifndef ARDUINO_ARCH_ESP8266
#include <freertos/FreeRTOS.h>
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
#ifndef ARDUINO_ARCH_ESP8266
  const char *host = nullptr;  // Borrowed only until the queued submission.
#endif
  uint32_t address = 0;
};
DnsSlot jobs[4];
void resolved(const char *, const ip_addr_t *address, void *context) {
  auto job = static_cast<DnsSlot *>(context);
  job->address = address && IP_IS_V4(address)
      ? ip4_addr_get_u32(ip_2_ip4(address)) : 0;
  if (job->state.exchange(Done) == Abandoned) job->state.store(Free);
}
void query(const char *host, DnsSlot *job) {
  ip_addr_t address;
  const err_t error = dns_gethostbyname(host, &address, resolved, job);
  if (error != ERR_INPROGRESS)
    resolved(nullptr, error == ERR_OK ? &address : nullptr, job);
}
#ifndef ARDUINO_ARCH_ESP8266
portMUX_TYPE hostnameLock = portMUX_INITIALIZER_UNLOCKED;
void submit(void *context) {
  auto job = static_cast<DnsSlot *>(context);
  char host[256];
  // stop()/destruction can free the owner on the SDK task. Protect only the
  // bounded copy, never DNS/lwIP calls or network waiting, with this lock.
  portENTER_CRITICAL(&hostnameLock);
  if (job->state.load() == Abandoned) {
    job->state.store(Free);
    portEXIT_CRITICAL(&hostnameLock);
    return;
  }
  memcpy(host, job->host, strlen(job->host) + 1);
  job->host = nullptr;
  portEXIT_CRITICAL(&hostnameLock);
  // lwIP copies the name before returning; only the static job survives.
  query(host, job);
}
#endif
}  // namespace

bool Supla::AsyncDns::start(const char *host) {
  cancel();
  if (!host || strlen(host) >= 256) return false;
  for (int i = 0; i < 4; ++i) {
    int expected = Free;
    if (!jobs[i].state.compare_exchange_strong(expected, Pending)) continue;
    jobs[i].address = 0;
    slot_ = i;
#ifdef ARDUINO_ARCH_ESP8266
    // ESP8266 submits synchronously on its cooperative lwIP context. There
    // is no queued reader, so even a temporary hostname needs no extra copy.
    query(host, &jobs[i]);
#else
    portENTER_CRITICAL(&hostnameLock);
    jobs[i].host = host;
    portEXIT_CRITICAL(&hostnameLock);
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
#ifndef ARDUINO_ARCH_ESP8266
  portENTER_CRITICAL(&hostnameLock);
#endif
  if (job.state.exchange(Abandoned) == Done) job.state.store(Free);
#ifndef ARDUINO_ARCH_ESP8266
  portEXIT_CRITICAL(&hostnameLock);
#endif
  slot_ = -1;
}
#endif
