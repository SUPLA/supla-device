// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp8266_async_client.h"
#if defined(ARDUINO_ARCH_ESP8266) && defined(ARDUINO)
#include <Arduino.h>
#include <BearSSLHelpers.h>
#include <StackThunk.h>
#include <bearssl/bearssl.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <supla/crypto.h>
#include <supla/log_wrapper.h>
#include <new>

// Use the same alternate BearSSL stack as the Arduino core. Crypto steps
// exceed the ordinary sketch stack even though network progress is bounded.
#define br_ssl_engine_recvapp_ack BearSSL::thunk_br_ssl_engine_recvapp_ack
#define br_ssl_engine_recvapp_buf BearSSL::thunk_br_ssl_engine_recvapp_buf
#define br_ssl_engine_recvrec_ack BearSSL::thunk_br_ssl_engine_recvrec_ack
#define br_ssl_engine_recvrec_buf BearSSL::thunk_br_ssl_engine_recvrec_buf
#define br_ssl_engine_sendapp_ack BearSSL::thunk_br_ssl_engine_sendapp_ack
#define br_ssl_engine_sendapp_buf BearSSL::thunk_br_ssl_engine_sendapp_buf
#define br_ssl_engine_sendrec_ack BearSSL::thunk_br_ssl_engine_sendrec_ack
#define br_ssl_engine_sendrec_buf BearSSL::thunk_br_ssl_engine_sendrec_buf

namespace {
// Implements the public BearSSL validator interface for the SDK's existing
// explicit fingerprint/insecure modes. CA mode uses x509_minimal instead.
struct LeafValidator {
  const br_x509_class *vtable;
  br_x509_decoder_context decoder;
  br_sha1_context digest;
  uint8_t fingerprint[20];
  bool checkFingerprint;
  bool complete;
};
void startChain(const br_x509_class **context, const char *) {
  auto leaf = reinterpret_cast<LeafValidator *>(context);
  br_x509_decoder_init(&leaf->decoder, nullptr, nullptr, nullptr, nullptr);
  br_sha1_init(&leaf->digest);
  leaf->complete = false;
}
void startCertificate(const br_x509_class **, uint32_t) {}
void appendCertificate(const br_x509_class **context, const unsigned char *data,
                        size_t size) {
  auto leaf = reinterpret_cast<LeafValidator *>(context);
  if (leaf->complete) return;
  br_sha1_update(&leaf->digest, data, size);
  br_x509_decoder_push(&leaf->decoder, data, size);
}
void endCertificate(const br_x509_class **context) {
  reinterpret_cast<LeafValidator *>(context)->complete = true;
}
unsigned endChain(const br_x509_class **context) {
  auto leaf = reinterpret_cast<LeafValidator *>(context);
  if (!leaf->complete || br_x509_decoder_last_error(&leaf->decoder))
    return BR_ERR_X509_INVALID_VALUE;
  uint8_t actual[20];
  br_sha1_out(&leaf->digest, actual);
  return leaf->checkFingerprint && !Supla::Crypto::constantTimeEqual(
      actual, leaf->fingerprint, sizeof(actual)) ? BR_ERR_X509_NOT_TRUSTED : 0;
}
const br_x509_pkey *publicKey(const br_x509_class *const *context,
                              unsigned *usages) {
  auto leaf = reinterpret_cast<const LeafValidator *>(context);
  if (usages) *usages = BR_KEYTYPE_KEYX | BR_KEYTYPE_SIGN;
  return br_x509_decoder_get_pkey(
      const_cast<br_x509_decoder_context *>(&leaf->decoder));
}
const br_x509_class leafVtable PROGMEM = {
sizeof(LeafValidator), startChain, startCertificate, appendCertificate,
endCertificate, endChain, publicKey};
// Once the handshake is verified, renegotiation is disabled. Keep a live
// fail-closed validator instead of a dangling reference to freed X.509 state.
void rejectStart(const br_x509_class **, const char *) {}
void rejectAppend(const br_x509_class **, const unsigned char *, size_t) {}
void rejectEnd(const br_x509_class **) {}
unsigned rejectChain(const br_x509_class **) { return BR_ERR_X509_NOT_TRUSTED; }
const br_x509_pkey *rejectKey(const br_x509_class *const *, unsigned *) {
  return nullptr;
}
const br_x509_class rejectVtable PROGMEM = {
sizeof(const br_x509_class *), rejectStart, startCertificate, rejectAppend,
rejectEnd, rejectChain, rejectKey};

int hexDigit(char value) {
  if (value >= '0' && value <= '9') return value - '0';
  if (value >= 'a' && value <= 'f') return value - 'a' + 10;
  if (value >= 'A' && value <= 'F') return value - 'A' + 10;
  return -1;
}
}  // namespace

struct Supla::Esp8266AsyncClient::Tls {
  br_ssl_client_context client;
  br_x509_minimal_context *minimal = nullptr;
  LeafValidator *leaf = nullptr;
  const br_x509_class *reject = &rejectVtable;
  uint8_t input[1024 + 325];
  uint8_t output[512 + 85];
  BearSSL::X509List *ca = nullptr;
  void releaseVerifier() {
    br_ssl_engine_set_x509(&client.eng, &reject);
    delete minimal;
    minimal = nullptr;
    delete leaf;
    leaf = nullptr;
    delete ca;
    ca = nullptr;
  }
  ~Tls() {
    releaseVerifier();
    volatile uint8_t *bytes = reinterpret_cast<volatile uint8_t *>(&client);
    for (size_t i = 0; i < sizeof(client); ++i) bytes[i] = 0;
    memset(input, 0, sizeof(input));
    memset(output, 0, sizeof(output));
  }
};

Supla::Esp8266AsyncClient::~Esp8266AsyncClient() { stop(); }

void Supla::Esp8266AsyncClient::setFingerprint(const char *value) {
  stop();
  if (value && strlen(value) >= sizeof(fingerprint_)) {
    fingerprint_[0] = '!';  // Invalid, never silently fall back to insecure.
    fingerprint_[1] = 0;
    return;
  }
  snprintf(fingerprint_, sizeof(fingerprint_), "%s", value ? value : "");
}

bool Supla::Esp8266AsyncClient::isCertificateValidationEnabled() const {
  return rootCACert || fingerprint_[0];
}

Supla::Client::ConnectResult Supla::Esp8266AsyncClient::startTransport(
    const char *host, uint16_t port) {
  failed_ = false;
  port_ = port;
  if (sslEnabled && rootCACert && time(nullptr) < 8 * 3600 * 2) {
    static bool ntpRequested = false;
    if (!ntpRequested) {
      configTime(0, 0, "pool.ntp.org", "time.nist.gov");
      ntpRequested = true;
    }
    return ConnectResult::Failed;
  }
  resolving_ = dns_.start(host);
  return resolving_ ? ConnectResult::Pending : ConnectResult::Failed;
}

err_t Supla::Esp8266AsyncClient::onConnected(void *context, tcp_pcb *,
                                            err_t error) {
  auto client = static_cast<Esp8266AsyncClient *>(context);
  client->tcpReady_ = error == ERR_OK;
  client->failed_ = error != ERR_OK;
  return ERR_OK;
}

err_t Supla::Esp8266AsyncClient::onReceive(void *context, tcp_pcb *, pbuf *data,
                                          err_t error) {
  auto client = static_cast<Esp8266AsyncClient *>(context);
  if (!data) {
    client->remoteClosed_ = true;
    return ERR_OK;
  }
  if (error != ERR_OK) return error;
  // Do not acknowledge more TCP bytes than the bounded receive window.
  if ((client->received_ ? client->received_->tot_len : 0) +
      data->tot_len > 4096)
    return ERR_MEM;
  if (client->received_) pbuf_cat(client->received_, data);
  else client->received_ = data;
  return ERR_OK;
}

void Supla::Esp8266AsyncClient::onError(void *context, err_t) {
  auto client = static_cast<Esp8266AsyncClient *>(context);
  client->pcb_ = nullptr;  // lwIP already freed it.
  client->failed_ = true;
}

Supla::Client::ConnectResult Supla::Esp8266AsyncClient::pollTransport() {
  if (failed_ || remoteClosed_) return ConnectResult::Failed;
  if (resolving_) {
    uint32_t address = 0;
    const int result = dns_.poll(&address);
    if (!result) return ConnectResult::Pending;
    if (result < 0) return ConnectResult::Failed;
    resolving_ = false;
    pcb_ = tcp_new();
    if (!pcb_) return ConnectResult::Failed;
    tcp_arg(pcb_, this);
    tcp_recv(pcb_, onReceive);
    tcp_err(pcb_, onError);
    ip_addr_t ip;
    ip_addr_set_ip4_u32(&ip, address);
    if (tcp_connect(pcb_, &ip, port_, onConnected) != ERR_OK)
      return ConnectResult::Failed;
    return ConnectResult::Pending;
  }
  if (!tcpReady_) return ConnectResult::Pending;
  srcIp = ip4_addr_get_u32(ip_2_ip4(&pcb_->local_ip));
  if (!sslEnabled) return ConnectResult::Connected;
  if (!tls_ && !setupTls()) return ConnectResult::Failed;
  pumpTls();
  if (failed_) return ConnectResult::Failed;
  const unsigned state = br_ssl_engine_current_state(&tls_->client.eng);
  if (state & (BR_SSL_SENDAPP | BR_SSL_RECVAPP)) {
    tls_->releaseVerifier();
    return ConnectResult::Connected;
  }
  return ConnectResult::Pending;
}

bool Supla::Esp8266AsyncClient::setupTls() {
  if (!stack_thunk_get_refcnt() && ESP.getMaxFreeBlockSize() < 6200)
    return false;
  stack_thunk_add_ref();
  thunkReferenced_ = true;
  tls_ = new (std::nothrow) Tls;
  if (!tls_) return false;
  if (rootCACert) {
    tls_->ca = new (std::nothrow) BearSSL::X509List(rootCACert);
    if (!tls_->ca || !tls_->ca->getCount()) return false;
  }
  tls_->minimal = new (std::nothrow) br_x509_minimal_context;
  if (!tls_->minimal) return false;
  br_ssl_client_init_full(&tls_->client, tls_->minimal,
      tls_->ca ? tls_->ca->getTrustAnchors() : nullptr,
      tls_->ca ? tls_->ca->getCount() : 0);
  auto engine = &tls_->client.eng;
  if (tls_->ca) {
    const uint32_t now = time(nullptr);
    br_x509_minimal_set_time(tls_->minimal, now / 86400 + 719528,
                            now % 86400);
  } else {
    tls_->leaf = new (std::nothrow) LeafValidator;
    if (!tls_->leaf) return false;
    auto &leaf = *tls_->leaf;
    memset(&leaf, 0, sizeof(leaf));
    leaf.vtable = &leafVtable;
    leaf.checkFingerprint = fingerprint_[0];
    if (leaf.checkFingerprint) {
      const char *text = fingerprint_;
      for (size_t i = 0; i < sizeof(leaf.fingerprint); ++i) {
        while (*text == ' ' || *text == ':') ++text;
        const int high = hexDigit(*text);
        if (high < 0) return false;
        ++text;
        const int low = hexDigit(*text);
        if (low < 0) return false;
        ++text;
        leaf.fingerprint[i] = high * 16 + low;
      }
      while (*text == ' ' || *text == ':') ++text;
      if (*text) return false;
    }
    br_ssl_engine_set_x509(engine, &leaf.vtable);
    delete tls_->minimal;
    tls_->minimal = nullptr;
  }
  br_ssl_engine_set_buffers_bidi(engine, tls_->input, sizeof(tls_->input),
                                tls_->output, sizeof(tls_->output));
  br_ssl_engine_set_versions(engine, BR_TLS12, BR_TLS12);
  br_ssl_engine_add_flags(engine, BR_OPT_NO_RENEGOTIATION);
  uint8_t entropy[32];
  if (!Supla::Crypto::fillRandom(entropy, sizeof(entropy))) return false;
  br_ssl_engine_inject_entropy(engine, entropy, sizeof(entropy));
  memset(entropy, 0, sizeof(entropy));
  return br_ssl_client_reset(&tls_->client, connectionHost(), 0);
}

void Supla::Esp8266AsyncClient::pumpTls() {
  if (!tls_ || !pcb_ || failed_) return;
  auto engine = &tls_->client.eng;
  // A fixed number of record-buffer steps; never wait for bytes or TCP ACKs.
  for (int step = 0; step < 4; ++step) {
    const unsigned state = br_ssl_engine_current_state(engine);
    if (state == BR_SSL_CLOSED) {
      SUPLA_LOG_WARNING("TLS connection closed error=%d",
                        br_ssl_engine_last_error(engine));
      failed_ = true;
      return;
    }
    size_t size = 0;
    if (state & BR_SSL_SENDREC) {
      auto data = br_ssl_engine_sendrec_buf(engine, &size);
      if (size > 512) size = 512;
      const int sent = sendTcp(data, size);
      if (sent > 0) {
        br_ssl_engine_sendrec_ack(engine, sent);
        continue;
      }
    }
    if ((state & BR_SSL_RECVREC) && received_) {
      auto data = br_ssl_engine_recvrec_buf(engine, &size);
      if (size > 512) size = 512;
      const int received = receiveTcp(data, size);
      if (received > 0) {
        br_ssl_engine_recvrec_ack(engine, received);
        continue;
      }
    }
    break;
  }
}

int Supla::Esp8266AsyncClient::sendTcp(const uint8_t *data, size_t size) {
  if (!pcb_ || failed_) return 0;
  if (size > tcp_sndbuf(pcb_)) size = tcp_sndbuf(pcb_);
  if (!size) return -1;
  const err_t result = tcp_write(pcb_, data, size, TCP_WRITE_FLAG_COPY);
  if (result == ERR_MEM) return -1;
  if (result != ERR_OK) {
    failed_ = true;
    return 0;
  }
  tcp_output(pcb_);
  return size;
}

int Supla::Esp8266AsyncClient::receiveTcp(uint8_t *data, size_t size) {
  if (!received_) return failed_ || remoteClosed_ ? 0 : -1;
  const size_t remaining = received_->tot_len - receiveOffset_;
  if (size > remaining) size = remaining;
  pbuf_copy_partial(received_, data, size, receiveOffset_);
  receiveOffset_ += size;
  if (pcb_) tcp_recved(pcb_, size);
  if (receiveOffset_ == received_->tot_len) {
    pbuf_free(received_);
    received_ = nullptr;
    receiveOffset_ = 0;
  }
  return size;
}

int Supla::Esp8266AsyncClient::available() {
  service();
  pumpTls();
  if (tls_) {
    size_t size = 0;
    br_ssl_engine_recvapp_buf(&tls_->client.eng, &size);
    return size;
  }
  return received_ ? received_->tot_len - receiveOffset_ : 0;
}

int Supla::Esp8266AsyncClient::readImp(uint8_t *data, size_t size) {
  service();
  if (!tls_) return receiveTcp(data, size);
  pumpTls();
  size_t available = 0;
  const auto input = br_ssl_engine_recvapp_buf(&tls_->client.eng, &available);
  if (!available) return failed_ ? 0 : -1;
  if (size > available) size = available;
  memcpy(data, input, size);
  br_ssl_engine_recvapp_ack(&tls_->client.eng, size);
  return size;
}

int Supla::Esp8266AsyncClient::transmit(const uint8_t *data, size_t size) {
  if (!tls_) return sendTcp(data, size);
  pumpTls();
  if (failed_) return 0;
  size_t room = 0;
  auto output = br_ssl_engine_sendapp_buf(&tls_->client.eng, &room);
  if (!room) return -1;
  if (size > room) size = room;
  memcpy(output, data, size);
  br_ssl_engine_sendapp_ack(&tls_->client.eng, size);
  br_ssl_engine_flush(&tls_->client.eng, 0);
  pumpTls();
  return failed_ ? 0 : size;
}

void Supla::Esp8266AsyncClient::service() {
  pumpTls();
  AsyncClient::service();
}

bool Supla::Esp8266AsyncClient::transportConnected() {
  return pcb_ && tcpReady_ && !failed_ &&
      (!remoteClosed_ || received_ || (tls_ &&
       (br_ssl_engine_current_state(&tls_->client.eng) & BR_SSL_RECVAPP)));
}

bool Supla::Esp8266AsyncClient::isSecure() const {
  return connectionReady() && sslEnabled && tls_ && !failed_;
}

void Supla::Esp8266AsyncClient::closeTransport() {
  dns_.cancel();
  if (pcb_) {
    tcp_arg(pcb_, nullptr);
    tcp_recv(pcb_, nullptr);
    tcp_err(pcb_, nullptr);
    tcp_abort(pcb_);
    pcb_ = nullptr;
  }
  if (received_) pbuf_free(received_);
  received_ = nullptr;
  receiveOffset_ = 0;
  tcpReady_ = false;
  remoteClosed_ = false;
  resolving_ = false;
  delete tls_;
  tls_ = nullptr;
  if (thunkReferenced_) stack_thunk_del_ref();
  thunkReferenced_ = false;
}
#endif
