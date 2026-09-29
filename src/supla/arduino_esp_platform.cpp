// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#if defined(ARDUINO_ARCH_ESP8266) || defined(ARDUINO_ARCH_ESP32)

#ifdef ARDUINO_ARCH_ESP8266
#include <stdlib.h>
#include <string.h>
#include <new>
#endif
#include <supla/log_wrapper.h>
#if defined(ARDUINO_ARCH_ESP32)
#include <esp_system.h>
#endif
#include <WiFiClient.h>
#include <WiFiClientSecure.h>
#include <supla/clock/clock.h>
#include <SuplaDevice.h>
#include "tools.h"
#include "supla/network/client.h"

namespace Supla {
#ifdef ARDUINO_ARCH_ESP8266
class ArduinoEspCaStore : public BearSSL::CertStoreBase {
 public:
  void setCertificates(BearSSL::X509List *certificates,
                       const char *sourcePem) {
    clear();
    this->certificates = certificates;
    this->sourcePem = sourcePem;
  }

  void finishConnectionAttempt() {
    // ArduinoEspClient owns and releases this list after connect() returns.
    if (!ownsCertificates) {
      certificates = nullptr;
    }
  }

  void clear() {
    if (ownsCertificates) {
      delete certificates;
    }
    certificates = nullptr;
    sourcePem = nullptr;
    ownsCertificates = false;
    releaseCertificatesAfterCallback = false;
  }

  void installCertStore(br_x509_minimal_context *context) override {
    br_x509_minimal_set_dynamic(context, this, findTrustAnchor,
                                freeTrustAnchor);
  }

  bool hasDuplicateDistinguishedNames() const {
    if (certificates == nullptr) {
      return false;
    }

    const br_x509_trust_anchor *anchors = certificates->getTrustAnchors();
    const size_t count = certificates->getCount();
    for (size_t i = 0; i < count; i++) {
      uint8_t currentHash[32];
      hashDistinguishedName(anchors[i], currentHash);
      for (size_t j = 0; j < i; j++) {
        uint8_t previousHash[32];
        hashDistinguishedName(anchors[j], previousHash);
        if (memcmp(currentHash, previousHash, sizeof(currentHash)) == 0) {
          return true;
        }
      }
    }
    return false;
  }

 private:
  static void hashDistinguishedName(const br_x509_trust_anchor &anchor,
                                   uint8_t hash[32]) {
    br_sha256_context context;
    br_sha256_init(&context);
    br_sha256_update(&context, anchor.dn.data, anchor.dn.len);
    br_sha256_out(&context, hash);
  }

  static const br_x509_trust_anchor *findTrustAnchor(
      void *context, void *hashedDn, size_t hashedDnLength) {
    auto *store = static_cast<ArduinoEspCaStore *>(context);
    if (store == nullptr || hashedDn == nullptr || hashedDnLength != 32) {
      return nullptr;
    }

    store->releaseCertificatesAfterCallback = false;
    if (store->certificates == nullptr) {
      if (store->sourcePem == nullptr) {
        return nullptr;
      }
      store->certificates =
          new (std::nothrow) BearSSL::X509List(store->sourcePem);
      if (store->certificates == nullptr ||
          store->certificates->getCount() == 0) {
        delete store->certificates;
        store->certificates = nullptr;
        return nullptr;
      }
      store->ownsCertificates = true;
      store->releaseCertificatesAfterCallback = true;
    }

    const br_x509_trust_anchor *anchors =
        store->certificates->getTrustAnchors();
    const size_t count = store->certificates->getCount();
    for (size_t i = 0; i < count; i++) {
      uint8_t candidateHash[32];
      hashDistinguishedName(anchors[i], candidateHash);
      if (memcmp(candidateHash, hashedDn, sizeof(candidateHash)) != 0) {
        continue;
      }

      auto *result = static_cast<br_x509_trust_anchor *>(
          calloc(1, sizeof(br_x509_trust_anchor)));
      if (result == nullptr) {
        store->releaseTemporaryCertificates();
        return nullptr;
      }
      result->dn.data = static_cast<unsigned char *>(malloc(hashedDnLength));
      if (result->dn.data == nullptr) {
        free(result);
        store->releaseTemporaryCertificates();
        return nullptr;
      }
      memcpy(result->dn.data, hashedDn, hashedDnLength);
      result->dn.len = hashedDnLength;
      result->flags = anchors[i].flags;
      // BearSSL uses the public key only until freeTrustAnchor() is called.
      result->pkey = anchors[i].pkey;
      return result;
    }
    store->releaseTemporaryCertificates();
    return nullptr;
  }

  static void freeTrustAnchor(void *context,
                              const br_x509_trust_anchor *anchor) {
    if (anchor == nullptr) {
      return;
    }
    free(anchor->dn.data);
    free(const_cast<br_x509_trust_anchor *>(anchor));
    auto *store = static_cast<ArduinoEspCaStore *>(context);
    if (store != nullptr) {
      store->releaseTemporaryCertificates();
    }
  }

  void releaseTemporaryCertificates() {
    if (releaseCertificatesAfterCallback && ownsCertificates) {
      delete certificates;
      certificates = nullptr;
      ownsCertificates = false;
      releaseCertificatesAfterCallback = false;
    }
  }

  BearSSL::X509List *certificates = nullptr;
  const char *sourcePem = nullptr;
  bool ownsCertificates = false;
  bool releaseCertificatesAfterCallback = false;
};
#endif  // ARDUINO_ARCH_ESP8266

class ArduinoEspClient : public Client {
 public:
  ~ArduinoEspClient() {
    if (clientSec) {
      destroySecureClient();
    }
    if (wifiClient) {
      wifiClient->stop();
      delete wifiClient;
      wifiClient = nullptr;
    }
  }

  int available() override {
    if (wifiClient) {
      return wifiClient->available();
    }
    return 0;
  }

  void stop() override {
    if (wifiClient) {
      wifiClient->stop();
    }
#ifdef ARDUINO_ARCH_ESP8266
    clearSecureAuthentication();
#endif
  }

  uint8_t connected() override {
    return (wifiClient != nullptr) && wifiClient->connected();
  }

  void setTimeoutMs(uint16_t _timeoutMs) override {
    timeoutMs = _timeoutMs;
  }

  void setServersCertFingerprint(String value) {
    fingerprint = value;
  }

 protected:
  int connectImp(const char *host, uint16_t port) override {
    stop();

    if (sslEnabled) {
#ifdef ARDUINO_ARCH_ESP8266
      clearSecureAuthentication();
#endif
      if (clientSec == nullptr) {
        if (wifiClient != nullptr) {
          delete wifiClient;
          wifiClient = nullptr;
        }
        clientSec = new WiFiClientSecure();
      }
      wifiClient = clientSec;

      wifiClient->setTimeout(timeoutMs);
#ifdef ARDUINO_ARCH_ESP8266
      clientSec->setBufferSizes(1024, 512);  // EXPERIMENTAL
      if (rootCACert) {
        // Set time via NTP, as required for x.509 validation
        static bool timeConfigured = Supla::Clock::IsReady();

        if (!timeConfigured) {
          timeConfigured = true;
          configTime(0, 0, "pool.ntp.org", "time.nist.gov");
          SUPLA_LOG_DEBUG("Waiting for NTP time sync");
          time_t now = time(nullptr);
          while (now < 8 * 3600 * 2) {
            delay(100);
            now = time(nullptr);
          }
        }

        caCert = new (std::nothrow) BearSSL::X509List(rootCACert);
        if (caCert == nullptr || caCert->getCount() == 0) {
          releaseCACert();
          SUPLA_LOG_ERROR("Failed to parse configured CA certificate");
          return 0;
        }
        caStore.setCertificates(caCert, rootCACert);
        if (caStore.hasDuplicateDistinguishedNames()) {
          // A dynamic lookup returns one anchor for a DN. Keep the full list
          // for this uncommon case so same-DN keys retain existing behavior.
          clientSec->setTrustAnchors(caCert);
          caCertIsStatic = true;
        } else {
          clientSec->setTrustAnchors(nullptr);
          clientSec->setCertStore(&caStore);
        }
      } else if (fingerprint.length() > 0) {
        if (!clientSec->setFingerprint(fingerprint.c_str())) {
          SUPLA_LOG_ERROR("Invalid TLS certificate fingerprint");
          return 0;
        }
      } else {
        clientSec->setInsecure();
      }
#else
      if (rootCACert) {
        clientSec->setCACert(rootCACert);
      } else {
        clientSec->setInsecure();
      }
#endif
    } else {
      destroySecureClient();
      if (wifiClient == nullptr) {
        wifiClient = new WiFiClient();
      }
    }

    int result = wifiClient->connect(host, port);
#ifdef ARDUINO_ARCH_ESP8266
    if (caCert != nullptr && !caCertIsStatic) {
      // BearSSL performs certificate validation synchronously in connect().
      // The store can parse it again if a later validation callback occurs.
      caStore.finishConnectionAttempt();
      releaseCACert();
    }
#endif
    if (result == 1) {
      srcIp = wifiClient->localIP();
      uint8_t ipArr[4];
      for (int i = 0; i < 4; i++) {
        ipArr[i] = (srcIp >> (i * 8)) & 0xFF;
      }

      SUPLA_LOG_DEBUG("Connected via IP %d.%d.%d.%d", ipArr[0], ipArr[1],
          ipArr[2], ipArr[3]);
    }
    if (clientSec) {
      char buf[200];
      int lastErr = 0;
#ifdef ARDUINO_ARCH_ESP8266
      lastErr = clientSec->getLastSSLError(buf, sizeof(buf));
#elif defined(ARDUINO_ARCH_ESP32)
      lastErr = clientSec->lastError(buf, sizeof(buf));
#endif

      if (lastErr) {
        SUPLA_LOG_ERROR("SSL error: %d, %s", lastErr, buf);
        if (sdc && (lastConnErr != lastErr)) {
          lastConnErr = lastErr;
          if (lastErr != 48) {
            sdc->addLastStateLog(buf);
          }
        }
      }
    }
#ifdef ARDUINO_ARCH_ESP8266
    if (result != 1 && caCertIsStatic) {
      // A failed handshake can still leave the client configured with this
      // pointer. Stop and detach before releasing the list, while keeping the
      // secure client available to retry.
      if (clientSec) {
        clientSec->stop();
      }
      clearSecureAuthentication();
    }
#endif
    return result;
  }

  void destroySecureClient() {
    if (clientSec) {
      clientSec->stop();
#ifdef ARDUINO_ARCH_ESP8266
      clearSecureAuthentication();
#endif
      if (wifiClient == clientSec) {
        wifiClient = nullptr;
      }
      delete clientSec;
      clientSec = nullptr;
    }
#ifdef ARDUINO_ARCH_ESP8266
    releaseCACert();
#endif
  }

#ifdef ARDUINO_ARCH_ESP8266
  void clearSecureAuthentication() {
    if (clientSec) {
      clientSec->setCertStore(nullptr);
      clientSec->setInsecure();
      clientSec->setTrustAnchors(nullptr);
    }
    caStore.clear();
    releaseCACert();
  }

  void releaseCACert() {
    if (caCert) {
      delete caCert;
      caCert = nullptr;
    }
    caCertIsStatic = false;
  }
#endif

  int readImp(uint8_t *buf, size_t count) override {
    if (wifiClient) {
      size_t size = wifiClient->available();

      if (size > 0) {
        if (size > count) {
          size = count;
        }
        return wifiClient->read(buf, size);
      }
    }
    return -1;
  }

  size_t writeImp(const uint8_t *buf, size_t count) override {
    if (wifiClient) {
      return wifiClient->write(buf, count);
    }
    return 0;
  }

  WiFiClient *wifiClient = nullptr;
  WiFiClientSecure *clientSec = nullptr;
#ifdef ARDUINO_ARCH_ESP8266
  BearSSL::X509List *caCert = nullptr;
  bool caCertIsStatic = false;
  ArduinoEspCaStore caStore;
#endif
  String fingerprint;
  uint16_t timeoutMs = 3000;
  int lastConnErr = 0;
};
};  // namespace Supla


void deviceSoftwareReset() {
  ESP.restart();
}

bool isDeviceSoftwareResetSupported() {
  return true;
}

bool isLastResetSoft() {
#ifdef ARDUINO_ARCH_ESP8266
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return resetInfo->reason == REASON_SOFT_RESTART;
#elif defined(ARDUINO_ARCH_ESP32)
  return esp_reset_reason() == ESP_RST_SW;
#else
  return false;
#endif
}

bool Supla::isLastResetPower() {
#ifdef ARDUINO_ARCH_ESP8266
  rst_info *resetInfo = ESP.getResetInfoPtr();
  return resetInfo->reason == REASON_DEFAULT_RST;
#elif defined(ARDUINO_ARCH_ESP32)
  return esp_reset_reason() == ESP_RST_POWERON;
#else
  return false;
#endif
}

Supla::Client *Supla::ClientBuilder() {
  return new Supla::ArduinoEspClient;
}

int Supla::getPlatformId() {
  // TODO(klew): do we need platfom id for Arduino based ESP SW?
  return 0;
}

void Supla::fillRandom(uint8_t *buffer, int size) {
#if defined(ARDUINO_ARCH_ESP8266)
  ESP.random(buffer, size);
#else
  esp_fill_random(buffer, size);
#endif
}

#endif
