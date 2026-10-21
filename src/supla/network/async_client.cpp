// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "async_client.h"
#ifndef ARDUINO_ARCH_AVR
#include <supla-common/proto.h>
#include <supla/time.h>
#include <stdlib.h>
#include <string.h>

struct Supla::AsyncClient::TxBlock {
  TxBlock *next;
  uint16_t used;
  uint16_t offset;
  bool sealed;
  uint8_t data[512];
};

Supla::AsyncClient::~AsyncClient() { clearWrites(); }

void Supla::AsyncClient::clearWrites() {
  while (head_) {
    auto next = head_->next;
    // The queue may contain registration secrets or forwarded credentials.
    volatile uint8_t *bytes = head_->data;
    for (size_t i = 0; i < sizeof(head_->data); ++i) bytes[i] = 0;
    free(head_);
    head_ = next;
  }
  tail_ = nullptr;
  queuedBytes_ = 0;
}

void Supla::AsyncClient::stop() {
  state_ = ConnectResult::Failed;
  synchronous_ = false;
  closeTransport();
  clearWrites();
  host_[0] = 0;
  srcIp = 0;
}

void Supla::AsyncClient::setTimeoutMs(uint16_t value) {
  timeoutMs = value ? value : 1;
}

void Supla::AsyncClient::setSSLEnabled(bool value) {
  if (sslEnabled != value) stop();
  Client::setSSLEnabled(value);
}

void Supla::AsyncClient::setCACert(const char *value) {
  // Callers may replace PEM bytes in the same storage buffer.
  stop();
  Client::setCACert(value);
}

Supla::Client::ConnectResult Supla::AsyncClient::connectStep(
    const char *host, uint16_t port) {
  synchronous_ = false;
  if (!host || !host[0] || strlen(host) >= sizeof(host_)) {
    stop();
    return ConnectResult::Failed;
  }
  if (state_ != ConnectResult::Failed &&
      (port != port_ || strcmp(host, host_))) stop();
  if (state_ == ConnectResult::Connected && !transportConnected()) {
    stop();
    return ConnectResult::Failed;
  }
  if (state_ == ConnectResult::Failed) {
    stop();
    memcpy(host_, host, strlen(host) + 1);
    port_ = port;
    startedMs_ = millis();
    connectionError_ = ConnectionError::NONE;
    logConnecting(host_, port);
    state_ = startTransport(host_, port);
  } else if (state_ == ConnectResult::Pending) {
    if (static_cast<uint32_t>(millis() - startedMs_) >= timeoutMs) {
      connectionError_ = ConnectionError::CONNECTION_TIMEOUT;
      stop();
      return ConnectResult::Failed;
    }
    state_ = pollTransport();
  }
  if (state_ == ConnectResult::Failed) {
    if (connectionError_ == ConnectionError::NONE)
      connectionError_ = ConnectionError::UNKNOWN;
    stop();
  }
  return state_;
}

// Explicit legacy connect() remains synchronous. SRPC/autodiscovery use steps.
int Supla::AsyncClient::connectImp(const char *host, uint16_t port) {
  stop();
  ConnectResult result;
  do {
    result = connectStep(host, port);
    if (result == ConnectResult::Pending) delay(1);
  } while (result == ConnectResult::Pending);
  synchronous_ = result == ConnectResult::Connected;
  return synchronous_ ? 1 : 0;
}

uint8_t Supla::AsyncClient::connected() {
  return state_ == ConnectResult::Connected && transportConnected();
}

bool Supla::AsyncClient::hasPendingWrites() const { return head_ != nullptr; }

Supla::ConnectionError Supla::AsyncClient::getConnectionError() const {
  return connectionError_;
}

void Supla::AsyncClient::service() {
  if (!head_ || state_ != ConnectResult::Connected) return;
  if (static_cast<uint32_t>(millis() - writeProgressMs_) >= timeoutMs) {
    connectionError_ = ConnectionError::CONNECTION_TIMEOUT;
    stop();
    return;
  }
  // Seal before first attempt: TLS WANT_WRITE retries must use identical bytes
  // and length, even when later SDK writes append to the queue.
  auto block = head_;
  block->sealed = true;
  const int sent = transmit(block->data + block->offset,
                            block->used - block->offset);
  if (sent == 0 || sent > block->used - block->offset) {
    connectionError_ = ConnectionError::CONNECTION_LOST;
    stop();
    return;
  }
  if (sent < 0) return;
  block->offset += sent;
  queuedBytes_ -= sent;
  writeProgressMs_ = millis();
  if (block->offset == block->used) {
    head_ = block->next;
    if (!head_) tail_ = nullptr;
    volatile uint8_t *bytes = block->data;
    for (size_t i = 0; i < sizeof(block->data); ++i) bytes[i] = 0;
    free(block);
  }
}

size_t Supla::AsyncClient::writeImp(const uint8_t *data, size_t size) {
  // Bound aggregate pending output by one maximal registration plus one packet.
  // Streaming registration emits many small writes, including on ESP8266.
  const size_t limit = sizeof(TDS_SuplaRegisterDevice_G) +
      sizeof(TSuplaDataPacket) + 2 * SUPLA_TAG_SIZE;
  if (!connected() || (!data && size) || size > limit - queuedBytes_) {
    stop();
    return 0;
  }
  const size_t total = size;
  if (!head_) writeProgressMs_ = millis();
  while (size) {
    if (!tail_ || tail_->sealed || tail_->used == sizeof(tail_->data)) {
      auto block = static_cast<TxBlock *>(calloc(1, sizeof(TxBlock)));
      if (!block) {
        stop();
        return 0;
      }
      if (tail_) tail_->next = block;
      else head_ = block;
      tail_ = block;
    }
    size_t take = sizeof(tail_->data) - tail_->used;
    if (take > size) take = size;
    memcpy(tail_->data + tail_->used, data, take);
    tail_->used += take;
    queuedBytes_ += take;
    data += take;
    size -= take;
  }
  // Explicit legacy connect()/write() retain their synchronous contract.
  if (synchronous_) {
    while (head_ && connected()) {
      service();
      if (head_) delay(1);
    }
    return connected() ? total : 0;
  }
  // Flushed by service(), not by every producer call: streaming registration
  // may append hundreds of Channels in one outer Device iteration.
  return total;
}
#endif  // !ARDUINO_ARCH_AVR
