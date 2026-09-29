// Copyright (C) AC SOFTWARE SP. Z.O.O.
// SPDX-License-Identifier: GPL-2.0-or-later
#ifndef EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_WIFICLIENTSECURE_H_
#define EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_WIFICLIENTSECURE_H_

#include <WiFiClient.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "tls_lifecycle_events.h"

struct br_x500_name {
  unsigned char *data;
  size_t len;
};

struct br_x509_pkey {
  unsigned char key_type;
  const unsigned char *key;
};

struct br_x509_trust_anchor {
  br_x500_name dn;
  unsigned char flags;
  br_x509_pkey pkey;
};

struct br_x509_minimal_context {
  void *dynamicContext = nullptr;
  const br_x509_trust_anchor *(*findTrustAnchor)(void *, void *, size_t) =
      nullptr;
  void (*freeTrustAnchor)(void *, const br_x509_trust_anchor *) = nullptr;
};

struct br_sha256_context {
  uint32_t value;
};

inline void br_sha256_init(br_sha256_context *context) {
  context->value = 2166136261u;
}

inline void br_sha256_update(br_sha256_context *context, const void *data,
                             size_t size) {
  const auto *bytes = static_cast<const uint8_t *>(data);
  for (size_t i = 0; i < size; i++) {
    context->value ^= bytes[i];
    context->value *= 16777619u;
  }
}

inline void br_sha256_out(const br_sha256_context *context, void *output) {
  auto *bytes = static_cast<uint8_t *>(output);
  for (size_t i = 0; i < 32; i++) {
    bytes[i] = static_cast<uint8_t>(context->value >> ((i % 4) * 8));
  }
}

inline void br_x509_minimal_set_dynamic(
    br_x509_minimal_context *context, void *dynamicContext,
    const br_x509_trust_anchor *(*find)(void *, void *, size_t),
    void (*release)(void *, const br_x509_trust_anchor *)) {
  context->dynamicContext = dynamicContext;
  context->findTrustAnchor = find;
  context->freeTrustAnchor = release;
}

namespace BearSSL {
class CertStoreBase {
 public:
  virtual ~CertStoreBase() = default;
  virtual void installCertStore(br_x509_minimal_context *context) = 0;
};

class X509List {
 public:
  explicit X509List(const char *source)
      : pem(source == nullptr ? "" : source), id(++nextId) {
    ++liveCount;
    parsePem(pem);
    TlsLifecycleEvents().push_back("anchor-create:" + pem);
    lastCreated = this;
  }

  ~X509List() {
    for (auto &anchor : anchors) {
      free(anchor.dn.data);
    }
    --liveCount;
    TlsLifecycleEvents().push_back("anchor-destroy:" + pem);
  }

  bool append(const char *source) {
    if (source == nullptr) {
      return false;
    }
    parsePem(source);
    return !anchors.empty();
  }

  size_t getCount() const { return anchors.size(); }

  const br_x509_trust_anchor *getTrustAnchors() const {
    return anchors.data();
  }

  std::string pem;
  int id;
  inline static int nextId = 0;
  inline static int liveCount = 0;
  inline static X509List *lastCreated = nullptr;

 private:
  void parsePem(const std::string &source) {
    if (source.empty() || source == "invalid") {
      return;
    }

    size_t start = 0;
    while (start < source.size()) {
      const size_t separator = source.find(';', start);
      const size_t end = separator == std::string::npos
                             ? source.size()
                             : separator;
      if (end > start) {
        const size_t length = end - start;
        auto *dn = static_cast<unsigned char *>(malloc(length));
        if (dn == nullptr) {
          return;
        }
        memcpy(dn, source.data() + start, length);
        br_x509_trust_anchor anchor{};
        anchor.dn.data = dn;
        anchor.dn.len = length;
        anchors.push_back(anchor);
      }
      if (separator == std::string::npos) {
        break;
      }
      start = separator + 1;
    }
  }

  std::vector<br_x509_trust_anchor> anchors;
};
}  // namespace BearSSL

class WiFiClientSecure : public WiFiClient {
 public:
  WiFiClientSecure() {
    ++liveCount;
    id = ++nextId;
    lastCreated = this;
    TlsLifecycleEvents().push_back("secure-create");
  }

  ~WiFiClientSecure() override {
    --liveCount;
    TlsLifecycleEvents().push_back("secure-destroy");
  }

  void stop() override {
    TlsLifecycleEvents().push_back("secure-stop");
    WiFiClient::stop();
  }

  int connect(const char *, uint16_t) override {
    if (certStore != nullptr) {
      br_x509_minimal_context context;
      certStore->installCertStore(&context);
      br_sha256_context hash;
      uint8_t hashedDn[32];
      br_sha256_init(&hash);
      if (lookupDn.empty()) {
        const auto *source = BearSSL::X509List::lastCreated;
        if (source == nullptr || source->getCount() == 0) {
          return 0;
        }
        const auto *sourceAnchor = source->getTrustAnchors();
        const size_t index = lookupIndex % source->getCount();
        br_sha256_update(&hash, sourceAnchor[index].dn.data,
                         sourceAnchor[index].dn.len);
      } else {
        br_sha256_update(&hash, lookupDn.data(), lookupDn.size());
      }
      br_sha256_out(&hash, hashedDn);
      const auto *dynamicAnchor =
          context.findTrustAnchor(context.dynamicContext, hashedDn,
                                  sizeof(hashedDn));
      if (dynamicAnchor == nullptr) {
        return 0;
      }
      context.freeTrustAnchor(context.dynamicContext, dynamicAnchor);
    }
    return connectResult;
  }

  void setBufferSizes(int, int) {}
  int getLastSSLError(char *, size_t) { return 0; }

  void setCertStore(BearSSL::CertStoreBase *store) {
    certStore = store;
    TlsLifecycleEvents().push_back(store == nullptr ? "set-store-null"
                                                   : "set-store");
  }

  void setTrustAnchors(const BearSSL::X509List *anchor) {
    trustAnchor = anchor;
    authMode = anchor == nullptr ? "" : "ca";
    TlsLifecycleEvents().push_back(anchor == nullptr ? "set-ca-null"
                                                    : "set-ca");
  }

  bool setFingerprint(const char *value) {
    if (!fingerprintValid) {
      return false;
    }
    trustAnchor = nullptr;
    fingerprint = value;
    authMode = "fingerprint";
    TlsLifecycleEvents().push_back("set-fingerprint");
    return true;
  }

  void setInsecure() {
    trustAnchor = nullptr;
    fingerprint.clear();
    authMode = "insecure";
    TlsLifecycleEvents().push_back("set-insecure");
  }

  const BearSSL::X509List *trustAnchor = nullptr;
  BearSSL::CertStoreBase *certStore = nullptr;
  std::string fingerprint;
  std::string authMode;
  int id = 0;
  inline static int nextId = 0;
  inline static WiFiClientSecure *lastCreated = nullptr;
  inline static int liveCount = 0;
  inline static int connectResult = 1;
  inline static int lookupIndex = 0;
  inline static std::string lookupDn;
  inline static bool fingerprintValid = true;
  inline static std::vector<std::string> &events = TlsLifecycleEvents();
};

#endif  // EXTRAS_TEST_ARDUINOESPPLATFORMTESTS_FAKES_WIFICLIENTSECURE_H_
