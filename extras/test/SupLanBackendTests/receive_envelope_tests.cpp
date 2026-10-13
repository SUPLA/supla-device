// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#include <gtest/gtest.h>
#include <suplan/suplan_data.h>
#include <suplan/suplan_fragment.h>
#include <suplan/suplan_wire.h>
#include <suplan_crypto_openssl.h>
#ifdef SUPLAN_TEST_BEARSSL
#include <suplan/suplan_crypto_bearssl.h>
using Backend = Supla::SupLan::BearSslCryptoPort;
#else
#include <suplan/suplan_crypto_mbedtls.h>
using Backend = Supla::SupLan::MbedTlsCryptoPort;
#endif
#include <cstring>
#include <vector>

namespace {
using namespace Supla::SupLan;  // NOLINT(build/namespaces)
TEST(ActualCryptoBackend, FragmentedMinimumRxEnvelopeAuthenticates) {
  Backend backend;
  OpenSslCryptoPort senderCrypto;
  DirectionalKeys keys = {};
  keys.trafficKey[0] = 7;
  for (size_t length : {768U, 769U, 992U}) {
    std::vector<uint8_t> input(length, 0x42);
    input[0] = kMessageClassNative;
    putUint32(input.data() + 1, 8);
    input[5] = kAckRequired;
    std::vector<uint8_t> frame(length + 32);
    frame[0] = kVersion;
    frame[1] = kFrameData;
    putUint64(frame.data() + 2, 9);
    putUint32(frame.data() + 10, 1);
    putUint16(frame.data() + 14, length);
    uint8_t nonce[12] = {};
    putUint32(nonce + 8, 1);
    ASSERT_TRUE(senderCrypto.aes128CcmEncrypt(keys.trafficKey, nonce,
        frame.data(), 16, input.data(), length, frame.data() + 16,
        frame.data() + 16 + length));
    std::vector<std::vector<uint8_t>> fragments;
    FragmentSender sender;
    ASSERT_TRUE(sender.send(frame.data(), frame.size(), 250, 1,
        [](void *context, const uint8_t *data, size_t size) {
          auto list = static_cast<decltype(fragments) *>(context);
          list->emplace_back(data, data + size);
          return true;
        }, &fragments));
    FragmentReassembler receiver;
    AdaptedFrameView received = {};
    for (const auto &fragment : fragments) {
      receiver.accept({1, 2016}, fragment.data(), fragment.size(), 0, 1000,
          [](void *, uint64_t id) { return id == 9; }, nullptr, &received);
    }
    ASSERT_EQ(received.length, frame.size());
    ReplayWindow replay;
    std::vector<uint8_t> plain(length);
    uint64_t session = 0;
    uint32_t sequence = 0;
    size_t size = 0;
    ASSERT_EQ(decodeProtectedData(&backend, &keys, received.data,
        received.length, &replay, plain.data(), plain.size(), &session,
        &sequence, &size), kProtectedDataOk) << length;
    EXPECT_EQ(plain, input);
    frame.back() ^= 1;
    EXPECT_EQ(decodeProtectedData(&backend, &keys, frame.data(), frame.size(),
        &replay, plain.data(), plain.size(), &session, &sequence, &size),
        kProtectedDataAuthenticationFailed);
    EXPECT_EQ(plain, std::vector<uint8_t>(length));
  }
}
TEST(ActualCryptoBackend, SeparateTransmitAndReceiveBounds) {
  Backend backend;
  uint8_t key[16] = {}, nonce[12] = {}, tag[16] = {};
  std::vector<uint8_t> input(kMaxReceiveApplicationBytes + 1, 0x42);
  std::vector<uint8_t> output(input.size(), 0x77);
  EXPECT_FALSE(backend.aes128CcmEncrypt(key, nonce, nullptr, 0, input.data(),
      SUPLAN_MAX_APPLICATION_BYTES + 1, output.data(), tag));
  EXPECT_FALSE(backend.aes128CcmDecrypt(key, nonce, nullptr, 0, input.data(),
      input.size(), tag, output.data()));
  EXPECT_EQ(output, std::vector<uint8_t>(input.size(), 0x77));
}
}  // namespace
