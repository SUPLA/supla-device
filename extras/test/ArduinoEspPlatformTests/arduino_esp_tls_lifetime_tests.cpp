// Copyright (C) AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <algorithm>
#include <cstring>
#include <string>

#include "../../../src/supla/arduino_esp_platform.cpp"  // NOLINT(build/include)

// Entropy is outside this TLS lifetime harness. Fail closed if requested.
bool Supla::Crypto::fillRandom(uint8_t *, size_t) {
  return false;
}

namespace {

void ResetFakeState() {
  ASSERT_EQ(0, WiFiClientSecure::liveCount);
  ASSERT_EQ(0, BearSSL::X509List::liveCount);
  WiFiClientSecure::lastCreated = nullptr;
  WiFiClientSecure::nextId = 0;
  WiFiClientSecure::lookupIndex = 0;
  WiFiClientSecure::lookupDn.clear();
  WiFiClientSecure::fingerprintValid = true;
  WiFiClientSecure::connectResult = 1;
  BearSSL::X509List::lastCreated = nullptr;
  BearSSL::X509List::nextId = 0;
  WiFiClientSecure::events.clear();
}

size_t EventIndex(const std::string &event) {
  const auto &events = WiFiClientSecure::events;
  const auto found = std::find(events.begin(), events.end(), event);
  EXPECT_NE(events.end(), found);
  return static_cast<size_t>(found - events.begin());
}

size_t LastEventIndex(const std::string &event) {
  const auto &events = WiFiClientSecure::events;
  const auto found = std::find(events.rbegin(), events.rend(), event);
  EXPECT_NE(events.rend(), found);
  return events.size() - 1 - (found - events.rbegin());
}

int ConnectWithCA(Supla::ArduinoEspClient *client, const char *ca) {
  client->setSSLEnabled(true);
  client->setCACert(ca);
  return client->connect("server", 443);
}

TEST(ArduinoEspTlsLifetime,
     ReleasesParsedCAAfterConnectAndReparsesOnReconnect) {
  ResetFakeState();
  Supla::ArduinoEspClient client;

  ASSERT_EQ(1, ConnectWithCA(&client, "ca-one"));
  auto *secureClient = WiFiClientSecure::lastCreated;
  ASSERT_NE(nullptr, secureClient->certStore);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(1, BearSSL::X509List::nextId);

  ASSERT_EQ(1, client.connect("server", 443));
  EXPECT_EQ(secureClient, WiFiClientSecure::lastCreated);
  EXPECT_NE(nullptr, secureClient->certStore);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(2, BearSSL::X509List::nextId);
}

TEST(ArduinoEspTlsLifetime, ChangedCAIsUsedOnNextConnection) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "ca-one"));

  client.setCACert("ca-two");
  ASSERT_EQ(1, client.connect("server", 443));

  EXPECT_EQ("anchor-create:ca-two", WiFiClientSecure::events[
      EventIndex("anchor-create:ca-two")]);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(2, BearSSL::X509List::nextId);
  EXPECT_EQ(1, WiFiClientSecure::liveCount);
}

TEST(ArduinoEspTlsLifetime, ChangedCAInSameBufferIsUsedOnReconnect) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  char ca[] = "ca-one";
  ASSERT_EQ(1, ConnectWithCA(&client, ca));

  std::memcpy(ca, "ca-two", sizeof(ca));
  client.setCACert(ca);
  ASSERT_EQ(1, client.connect("server", 443));

  EXPECT_EQ("anchor-create:ca-two", WiFiClientSecure::events[
      EventIndex("anchor-create:ca-two")]);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(2, BearSSL::X509List::nextId);
}

TEST(ArduinoEspTlsLifetime, DynamicLookupCanSelectLaterCAInList) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  WiFiClientSecure::lookupIndex = 1;

  ASSERT_EQ(1, ConnectWithCA(&client, "ca-one;ca-two"));
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(1, WiFiClientSecure::liveCount);
}

TEST(ArduinoEspTlsLifetime,
     FailedConnectionDetachesAndReleasesStaticTrustAnchors) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  WiFiClientSecure::connectResult = 0;

  EXPECT_EQ(0, ConnectWithCA(&client, "same;same"));
  EXPECT_EQ(1, WiFiClientSecure::liveCount);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(nullptr, WiFiClientSecure::lastCreated->trustAnchor);
  EXPECT_LT(LastEventIndex("secure-stop"), LastEventIndex("set-ca-null"));
  EXPECT_LT(LastEventIndex("set-ca-null"),
            EventIndex("anchor-destroy:same;same"));
}

TEST(ArduinoEspTlsLifetime, MissingDynamicAnchorFailsAndReleasesParsedCA) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  WiFiClientSecure::lookupDn = "unknown-issuer";

  EXPECT_EQ(0, ConnectWithCA(&client, "ca-one"));
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(1, WiFiClientSecure::liveCount);
}

TEST(ArduinoEspTlsLifetime,
     DuplicateDistinguishedNamesUseStaticListUntilStopThenRelease) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "same;same"));
  auto *secureClient = WiFiClientSecure::lastCreated;

  EXPECT_EQ(nullptr, secureClient->certStore);
  ASSERT_NE(nullptr, secureClient->trustAnchor);
  EXPECT_EQ(1, BearSSL::X509List::liveCount);

  client.stop();
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(nullptr, secureClient->trustAnchor);
}

TEST(ArduinoEspTlsLifetime, CAtoFingerprintDetachesDynamicStore) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "ca-one"));

  client.setCACert(nullptr);
  client.setServersCertFingerprint("fingerprint");
  ASSERT_EQ(1, client.connect("server", 443));

  auto *secureClient = WiFiClientSecure::lastCreated;
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ("fingerprint", secureClient->authMode);
  EXPECT_EQ(nullptr, secureClient->certStore);
  EXPECT_EQ(nullptr, secureClient->trustAnchor);
}

TEST(ArduinoEspTlsLifetime,
     StaticCAtoFingerprintDetachesAndReleasesTrustAnchor) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "same;same"));
  auto *secureClient = WiFiClientSecure::lastCreated;
  ASSERT_NE(nullptr, secureClient->trustAnchor);
  ASSERT_EQ(1, BearSSL::X509List::liveCount);

  client.setCACert(nullptr);
  client.setServersCertFingerprint("fingerprint");
  ASSERT_EQ(1, client.connect("server", 443));

  EXPECT_EQ(secureClient, WiFiClientSecure::lastCreated);
  EXPECT_EQ("fingerprint", secureClient->authMode);
  EXPECT_EQ(nullptr, secureClient->trustAnchor);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_LT(EventIndex("secure-stop"), EventIndex("set-ca-null"));
  EXPECT_LT(EventIndex("set-ca-null"),
            EventIndex("anchor-destroy:same;same"));
}

TEST(ArduinoEspTlsLifetime, CAtoInsecureDetachesDynamicStore) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "ca-one"));

  client.setCACert(nullptr);
  client.setServersCertFingerprint("");
  ASSERT_EQ(1, client.connect("server", 443));

  auto *secureClient = WiFiClientSecure::lastCreated;
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ("insecure", secureClient->authMode);
  EXPECT_EQ(nullptr, secureClient->certStore);
  EXPECT_EQ(nullptr, secureClient->trustAnchor);
}

TEST(ArduinoEspTlsLifetime, StaticCAtoInsecureDetachesAndReleasesTrustAnchor) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "same;same"));
  auto *secureClient = WiFiClientSecure::lastCreated;
  ASSERT_NE(nullptr, secureClient->trustAnchor);
  ASSERT_EQ(1, BearSSL::X509List::liveCount);

  client.setCACert(nullptr);
  client.setServersCertFingerprint("");
  ASSERT_EQ(1, client.connect("server", 443));

  EXPECT_EQ(secureClient, WiFiClientSecure::lastCreated);
  EXPECT_EQ("insecure", secureClient->authMode);
  EXPECT_EQ(nullptr, secureClient->trustAnchor);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_LT(EventIndex("secure-stop"), EventIndex("set-ca-null"));
  EXPECT_LT(EventIndex("set-ca-null"),
            EventIndex("anchor-destroy:same;same"));
}

TEST(ArduinoEspTlsLifetime, InvalidFingerprintDoesNotFallBackToInsecure) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  client.setSSLEnabled(true);
  client.setServersCertFingerprint("invalid");
  WiFiClientSecure::fingerprintValid = false;

  EXPECT_EQ(0, client.connect("server", 443));
  EXPECT_EQ("", WiFiClientSecure::lastCreated->authMode);
}

TEST(ArduinoEspTlsLifetime, DisablingSSLReleasesTLSClientAndCA) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "ca-one"));
  ASSERT_EQ(1, WiFiClientSecure::liveCount);

  client.setSSLEnabled(false);
  ASSERT_EQ(1, client.connect("server", 80));

  EXPECT_EQ(0, WiFiClientSecure::liveCount);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
}

TEST(ArduinoEspTlsLifetime,
     DisablingSSLWithStaticCAReleasesAnchorAfterDetachingClient) {
  ResetFakeState();
  Supla::ArduinoEspClient client;
  ASSERT_EQ(1, ConnectWithCA(&client, "same;same"));
  ASSERT_EQ(1, WiFiClientSecure::liveCount);
  ASSERT_EQ(1, BearSSL::X509List::liveCount);

  client.setSSLEnabled(false);
  ASSERT_EQ(1, client.connect("server", 80));

  EXPECT_EQ(0, WiFiClientSecure::liveCount);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_LT(EventIndex("secure-stop"), EventIndex("set-ca-null"));
  EXPECT_LT(EventIndex("set-ca-null"),
            EventIndex("anchor-destroy:same;same"));
  EXPECT_LT(EventIndex("anchor-destroy:same;same"),
            EventIndex("secure-destroy"));
}

TEST(ArduinoEspTlsLifetime,
     DestructorDetachesStaticAnchorBeforeReleasingItAndClient) {
  ResetFakeState();
  {
    Supla::ArduinoEspClient client;
    ASSERT_EQ(1, ConnectWithCA(&client, "same;same"));
    EXPECT_EQ(1, BearSSL::X509List::liveCount);
  }

  EXPECT_EQ(0, WiFiClientSecure::liveCount);
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_LT(LastEventIndex("secure-stop"), LastEventIndex("set-ca-null"));
  EXPECT_LT(LastEventIndex("set-ca-null"),
            EventIndex("anchor-destroy:same;same"));
  EXPECT_LT(EventIndex("anchor-destroy:same;same"),
            EventIndex("secure-destroy"));
}

TEST(ArduinoEspTlsLifetime, InvalidCAFailsClosedAndReleasesPartialState) {
  ResetFakeState();
  Supla::ArduinoEspClient client;

  EXPECT_EQ(0, ConnectWithCA(&client, "invalid"));
  EXPECT_EQ(0, BearSSL::X509List::liveCount);
  EXPECT_EQ(1, WiFiClientSecure::liveCount);
}

}  // namespace
