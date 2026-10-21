// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <supla/network/async_dns.h>
#include <lwip/dns.h>
#include <deque>
#ifndef ARDUINO_ARCH_ESP8266
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <thread>
#endif
#include <memory>
#include <string>
#include <utility>

namespace {
std::deque<std::pair<void (*)(void *), void *>> queued;
struct Query { std::string host; dns_found_callback callback; void *context; };
std::deque<Query> queries;
bool failSubmit = false;
bool immediate = false;
#ifndef ARDUINO_ARCH_ESP8266
std::function<void(const char *)> lookupHook;
#endif
void runSubmissions() {
  while (!queued.empty()) {
    auto work = queued.front();
    queued.pop_front();
    work.first(work.second);
  }
}
void answer(uint32_t address) {
  ASSERT_FALSE(queries.empty());
  auto query = queries.front();
  queries.pop_front();
  ip_addr_t ip = {address};
  query.callback(query.host.c_str(), address ? &ip : nullptr, query.context);
}
class DnsLifetime : public ::testing::Test {
  void TearDown() override {
    runSubmissions();
    while (!queries.empty()) answer(0);
    failSubmit = immediate = false;
#ifndef ARDUINO_ARCH_ESP8266
    lookupHook = {};
#endif
  }
};
}  // namespace

err_t tcpip_try_callback(void (*callback)(void *), void *context) {
  if (failSubmit) return ERR_MEM;
  queued.emplace_back(callback, context);
  return ERR_OK;
}
err_t dns_gethostbyname(const char *host, ip_addr_t *address,
                        dns_found_callback callback, void *context) {
#ifndef ARDUINO_ARCH_ESP8266
  if (lookupHook) lookupHook(host);
#endif
  if (immediate) {
    address->address = 1234;
    return ERR_OK;
  }
  queries.push_back({host, callback, context});
  return ERR_INPROGRESS;
}

TEST_F(DnsLifetime, DestructionBeforeSubmissionNeverDereferencesOwner) {
  {
    const std::string host(255, 'a');
    Supla::AsyncDns dns;
    ASSERT_TRUE(dns.start(host.c_str()));
  }  // Both owner and non-literal hostname are gone before queued submission.
  runSubmissions();
#ifdef ARDUINO_ARCH_ESP8266
  // Already submitted directly, with a copied name.
  ASSERT_EQ(queries.size(), 1U);
  answer(111);  // Cancelled result must only reclaim the static slot.
#else
  EXPECT_TRUE(queries.empty());
#endif
  Supla::AsyncDns next;
  ASSERT_TRUE(next.start("new"));
  runSubmissions();
  answer(42);
  uint32_t address = 0;
  ASSERT_EQ(next.poll(&address), 1);
  EXPECT_EQ(address, 42U);
}

TEST_F(DnsLifetime, StaleCallbackCannotCompleteNewAttempt) {
  Supla::AsyncDns dns;
  ASSERT_TRUE(dns.start("old"));
  runSubmissions();
  ASSERT_TRUE(dns.start("new"));
  runSubmissions();
  answer(111);
  uint32_t address = 0;
  EXPECT_EQ(dns.poll(&address), 0);
  answer(222);
  ASSERT_EQ(dns.poll(&address), 1);
  EXPECT_EQ(address, 222U);
}

TEST_F(DnsLifetime, CancelledOutstandingJobsBoundCapacityAndReclaim) {
  for (int i = 0; i < 4; ++i) {
    Supla::AsyncDns dns;
    ASSERT_TRUE(dns.start("cancelled"));
    runSubmissions();
  }
  Supla::AsyncDns dns;
  EXPECT_FALSE(dns.start("full"));
  answer(111);
  ASSERT_TRUE(dns.start("reclaimed"));
  runSubmissions();
  for (int i = 0; i < 3; ++i) answer(222);
  uint32_t address = 0;
  EXPECT_EQ(dns.poll(&address), 0);
  answer(333);
  ASSERT_EQ(dns.poll(&address), 1);
  EXPECT_EQ(address, 333U);
}

TEST_F(DnsLifetime, CachedResultAndSubmissionFailureReclaimSlots) {
  Supla::AsyncDns dns;
#ifndef ARDUINO_ARCH_ESP8266
  failSubmit = true;
  EXPECT_FALSE(dns.start("failure"));
  failSubmit = false;
#endif
  immediate = true;
  for (int i = 0; i < 20; ++i) {
    ASSERT_TRUE(dns.start("cached"));
    runSubmissions();
    uint32_t address = 0;
    ASSERT_EQ(dns.poll(&address), 1);
    EXPECT_EQ(address, 1234U);
  }
}

TEST_F(DnsLifetime, SubmittedQueryDoesNotRetainCallerHostname) {
  Supla::AsyncDns dns;
  {
    const std::string host(255, 'h');
    ASSERT_TRUE(dns.start(host.c_str()));
    runSubmissions();
    ASSERT_EQ(queries.size(), 1U);
    EXPECT_EQ(queries.front().host, host);
  }  // lwIP owns the query name now; caller storage is not used by completion.
  answer(789);
  uint32_t address = 0;
  ASSERT_EQ(dns.poll(&address), 1);
  EXPECT_EQ(address, 789U);
}

TEST_F(DnsLifetime, ReentryAfterCancelNeverReadsPriorOwnerStorage) {
  Supla::AsyncDns dns;
  {
    const std::string host(255, 'a');
    ASSERT_TRUE(dns.start(host.c_str()));
    dns.cancel();
  }
  const std::string replacement(255, 'b');
  ASSERT_TRUE(dns.start(replacement.c_str()));
  runSubmissions();
#ifdef ARDUINO_ARCH_ESP8266
  ASSERT_EQ(queries.size(), 2U);
  answer(111);
#else
  ASSERT_EQ(queries.size(), 1U);
#endif
  uint32_t address = 0;
  EXPECT_EQ(dns.poll(&address), 0);
  EXPECT_EQ(queries.front().host, replacement);
  answer(222);
  ASSERT_EQ(dns.poll(&address), 1);
  EXPECT_EQ(address, 222U);
}

TEST_F(DnsLifetime, RejectsOverlengthNameWithoutTruncation) {
  Supla::AsyncDns dns;
  const std::string host(256, 'x');
  EXPECT_FALSE(dns.start(host.c_str()));
  runSubmissions();
  EXPECT_TRUE(queries.empty());
}

#ifndef ARDUINO_ARCH_ESP8266
TEST_F(DnsLifetime, CancelDoesNotWaitForDnsOrFreeItsTemporaryInput) {
  Supla::AsyncDns dns;
  auto host = std::make_unique<std::string>(255, 'z');
  const std::string expected = *host;
  ASSERT_TRUE(dns.start(host->c_str()));
  std::mutex mutex;
  std::condition_variable condition;
  bool entered = false;
  bool release = false;
  std::string inputAfterCancel;
  lookupHook = [&](const char *input) {
    std::unique_lock<std::mutex> lock(mutex);
    entered = true;
    condition.notify_all();
    condition.wait(lock, [&] { return release; });
    inputAfterCancel = input;
  };
  std::thread tcpip(runSubmissions);
  {
    std::unique_lock<std::mutex> lock(mutex);
    condition.wait(lock, [&] { return entered; });
  }
  auto cancelled = std::async(std::launch::async, [&] {
    dns.cancel();
    host.reset();
  });
  // Cancellation must not wait for query work beyond the bounded name copy.
  EXPECT_EQ(cancelled.wait_for(std::chrono::seconds(1)),
            std::future_status::ready);
  {
    std::lock_guard<std::mutex> lock(mutex);
    release = true;
  }
  condition.notify_all();
  tcpip.join();
  cancelled.get();
  EXPECT_EQ(inputAfterCancel, expected);
  ASSERT_EQ(queries.size(), 1U);
  EXPECT_EQ(queries.front().host, expected);
  answer(123);  // Detached completion cannot access the destroyed hostname.
}
#endif
