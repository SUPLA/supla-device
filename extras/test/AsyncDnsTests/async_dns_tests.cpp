// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <supla/network/async_dns.h>
#include <lwip/dns.h>
#include <deque>
#include <memory>
#include <string>
#include <utility>

namespace {
std::deque<std::pair<void (*)(void *), void *>> queued;
struct Query { std::string host; dns_found_callback callback; void *context; };
std::deque<Query> queries;
bool failSubmit = false;
bool immediate = false;
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
  if (immediate) {
    address->address = 1234;
    return ERR_OK;
  }
  queries.push_back({host, callback, context});
  return ERR_INPROGRESS;
}

TEST_F(DnsLifetime, DestructionBeforeSubmissionNeverDereferencesOwner) {
  {
    Supla::AsyncDns dns;
    ASSERT_TRUE(dns.start("old"));
  }
  runSubmissions();
  EXPECT_TRUE(queries.empty());
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
  failSubmit = true;
  EXPECT_FALSE(dns.start("failure"));
  failSubmit = false;
  immediate = true;
  for (int i = 0; i < 20; ++i) {
    ASSERT_TRUE(dns.start("cached"));
    runSubmissions();
    uint32_t address = 0;
    ASSERT_EQ(dns.poll(&address), 1);
    EXPECT_EQ(address, 1234U);
  }
}
