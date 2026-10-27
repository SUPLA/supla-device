// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>
#include <supla/protocol/suplan_protocol.h>
#include <suplan/suplan_runtime.h>
#include <suplan_udp_linux.h>

#include <type_traits>

#ifdef SUPLAN_TESTING
#error "This target must compile the production API without test support"
#endif

namespace {

using Yes = std::true_type;
using No = std::false_type;

template <typename T, typename = void>
struct HasFaultHooks : No {};
template <typename T>
struct HasFaultHooks<T, std::void_t<decltype(&T::testHooks)>> : Yes {};
template <typename T, typename = void>
struct HasFlood : No {};
template <typename T>
struct HasFlood<T, std::void_t<decltype(&T::startFlood)>> : Yes {};
template <typename T, typename = void>
struct HasTxTap : No {};
template <typename T>
struct HasTxTap<T, std::void_t<decltype(&T::setDataTransmitObserver)>> : Yes {};
template <typename T, typename = void>
struct HasCapture : No {};
template <typename T>
struct HasCapture<T, std::void_t<decltype(&T::captureNextData)>> : Yes {};
template <typename T, typename = void>
struct HasReplay : No {};
template <typename T>
struct HasReplay<T, std::void_t<decltype(&T::replayCapturedData)>> : Yes {};
template <typename T, typename = void>
struct HasProbe : No {};
template <typename T>
struct HasProbe<T, std::void_t<decltype(&T::beginMulticastSelfTest)>> : Yes {};

TEST(SupLanProduction, NoFaultInjectionOrProtectedPacketCaptureApi) {
  EXPECT_FALSE(HasFaultHooks<Supla::SupLan::Runtime>::value);
  EXPECT_FALSE(HasFlood<Supla::SupLan::Runtime>::value);
  EXPECT_FALSE(HasTxTap<Supla::SupLan::Runtime>::value);
  EXPECT_FALSE(HasCapture<Supla::SupLan::LinuxUdpPort>::value);
  EXPECT_FALSE(HasReplay<Supla::SupLan::LinuxUdpPort>::value);
  EXPECT_FALSE(HasProbe<Supla::SupLan::LinuxUdpPort>::value);
}

TEST(SupLanProduction, AdapterOmitsFixtureMappingConstructor) {
  using Adapter = Supla::Protocol::SupLan;
  constexpr bool serverConstructor = std::is_constructible<Adapter,
      SuplaDeviceClass *, Supla::SupLan::PeerTable *>::value;
  // Old fixture constructor: (device, peers, nullptr, uint8_t count).
  constexpr bool fixtureConstructor = std::is_constructible<Adapter,
      SuplaDeviceClass *, Supla::SupLan::PeerTable *, std::nullptr_t,
      uint8_t>::value;
  EXPECT_TRUE(serverConstructor);
  EXPECT_FALSE(fixtureConstructor);
}

TEST(SupLanProduction, RuntimeLinksAndInitializesWithoutTestState) {
  Supla::SupLan::PeerTable peers;
  Supla::SupLan::Runtime runtime(nullptr, nullptr, nullptr, &peers,
                                {Supla::SupLan::kNodeIdDevice, 0}, 29);
  EXPECT_FALSE(runtime.initialized());
  EXPECT_EQ(runtime.poolDiagnostics().sessions.used, 0U);
}

}  // namespace
