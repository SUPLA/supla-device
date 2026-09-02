// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_idf_ota_mock.h"

#include <esp_http_client.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <supla/rsa_verificator.h>

#include <algorithm>
#include <cstring>
#include <string>
#include <vector>

struct esp_http_client {
  size_t responseIndex = 0;
  size_t offset = 0;
  size_t chunkIndex = 0;
  size_t chunkOffset = 0;
  size_t scriptedReadIndex = 0;
};

namespace {

struct HttpResponse {
  HttpResponse(const std::string &body, const std::vector<size_t> &chunks)
      : body(body), chunks(chunks) {
  }

  std::string body;
  std::vector<size_t> chunks;
  std::vector<int> scriptedReads;
  size_t readCallCount = 0;
  esp_err_t openResult = ESP_OK;
  int64_t fetchHeadersResult = 0;
  bool useFetchHeadersResult = false;
  int statusCode = 200;
  int64_t contentLength = -1;
  bool responseComplete = true;
  int errnoCode = 0;
  int tlsCode = 0;
  int tlsFlags = 0;
};

struct MockState {
  std::vector<HttpResponse> responses;
  std::string requestBody;
  std::string requestUrl;
  std::vector<uint8_t> partitionData;
  size_t httpClientInitCount = 0;
  bool certificateConfigured = false;
  size_t otaBeginCount = 0;
  size_t otaEndCount = 0;
  size_t otaAbortCount = 0;
  esp_err_t otaEndResult = ESP_OK;
  bool rsaVerificationResult = false;
  bool rsaVerificationCalled = false;
  bool bootPartitionSet = false;
};

MockState state;
esp_partition_t updatePartition = {};

HttpResponse &getResponse(esp_http_client_handle_t client) {
  return state.responses[client->responseIndex];
}

}  // namespace

void EspIdfOtaMock::reset() {
  state = {};
}

void EspIdfOtaMock::setHttpResponse(const std::string &body,
                                    const std::vector<size_t> &chunks) {
  state.responses = {HttpResponse(body, chunks)};
}

void EspIdfOtaMock::addHttpResponse(const std::string &body,
                                    const std::vector<size_t> &chunks) {
  state.responses.emplace_back(body, chunks);
}

void EspIdfOtaMock::setHttpReadResults(size_t responseIndex,
                                       const std::vector<int> &results) {
  state.responses.at(responseIndex).scriptedReads = results;
}

void EspIdfOtaMock::setHttpOpenError(size_t responseIndex,
                                     int tlsCode,
                                     int tlsFlags) {
  auto &response = state.responses.at(responseIndex);
  response.openResult = ESP_FAIL;
  response.tlsCode = tlsCode;
  response.tlsFlags = tlsFlags;
}

void EspIdfOtaMock::setHttpHeadersError(size_t responseIndex,
                                        int tlsCode,
                                        int tlsFlags) {
  auto &response = state.responses.at(responseIndex);
  response.fetchHeadersResult = ESP_FAIL;
  response.useFetchHeadersResult = true;
  response.tlsCode = tlsCode;
  response.tlsFlags = tlsFlags;
}

void EspIdfOtaMock::setHttpStatusCode(size_t responseIndex, int statusCode) {
  state.responses.at(responseIndex).statusCode = statusCode;
}

void EspIdfOtaMock::setHttpContentLength(size_t responseIndex,
                                         int64_t contentLength) {
  state.responses.at(responseIndex).contentLength = contentLength;
}

void EspIdfOtaMock::setResponseComplete(bool complete) {
  for (auto &response : state.responses) {
    response.responseComplete = complete;
  }
}

void EspIdfOtaMock::setResponseComplete(size_t responseIndex, bool complete) {
  state.responses.at(responseIndex).responseComplete = complete;
}

void EspIdfOtaMock::setOtaEndResult(int result) {
  state.otaEndResult = result;
}

void EspIdfOtaMock::setRsaVerificationResult(bool result) {
  state.rsaVerificationResult = result;
}

const std::string &EspIdfOtaMock::getRequestBody() {
  return state.requestBody;
}

const std::string &EspIdfOtaMock::getRequestUrl() {
  return state.requestUrl;
}

bool EspIdfOtaMock::wasCertificateConfigured() {
  return state.certificateConfigured;
}

size_t EspIdfOtaMock::getHttpClientInitCount() {
  return state.httpClientInitCount;
}

size_t EspIdfOtaMock::getHttpReadCallCount(size_t responseIndex) {
  return state.responses.at(responseIndex).readCallCount;
}

size_t EspIdfOtaMock::getOtaWrittenBytes() {
  return state.partitionData.size();
}

size_t EspIdfOtaMock::getOtaBeginCount() {
  return state.otaBeginCount;
}

size_t EspIdfOtaMock::getOtaEndCount() {
  return state.otaEndCount;
}

size_t EspIdfOtaMock::getOtaAbortCount() {
  return state.otaAbortCount;
}

bool EspIdfOtaMock::wasOtaBeginCalled() {
  return state.otaBeginCount != 0;
}

bool EspIdfOtaMock::wasOtaEndCalled() {
  return state.otaEndCount != 0;
}

bool EspIdfOtaMock::wasOtaAbortCalled() {
  return state.otaAbortCount != 0;
}

bool EspIdfOtaMock::wasRsaVerificationCalled() {
  return state.rsaVerificationCalled;
}

bool EspIdfOtaMock::wasBootPartitionSet() {
  return state.bootPartitionSet;
}

extern "C" {

esp_http_client_handle_t esp_http_client_init(
    const esp_http_client_config_t *config) {
  if (config == nullptr) {
    return nullptr;
  }
  state.requestUrl = config->url ? config->url : "";
  state.certificateConfigured = config->cert_pem != nullptr;
  if (state.httpClientInitCount >= state.responses.size()) {
    return nullptr;
  }
  auto client = new esp_http_client;
  client->responseIndex = state.httpClientInitCount++;
  return client;
}

esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client) {
  delete client;
  return ESP_OK;
}

esp_err_t esp_http_client_set_method(esp_http_client_handle_t,
                                     esp_http_client_method_t) {
  return ESP_OK;
}

esp_err_t esp_http_client_set_header(esp_http_client_handle_t,
                                     const char *,
                                     const char *) {
  return ESP_OK;
}

esp_err_t esp_http_client_open(esp_http_client_handle_t client, int) {
  return getResponse(client).openResult;
}

int esp_http_client_write(esp_http_client_handle_t,
                          const char *data,
                          int length) {
  if (data == nullptr || length < 0) {
    return -1;
  }
  state.requestBody.assign(data, static_cast<size_t>(length));
  return length;
}

int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client) {
  auto &response = getResponse(client);
  if (response.useFetchHeadersResult) {
    return response.fetchHeadersResult;
  }
  return static_cast<int64_t>(response.body.size());
}

int esp_http_client_read(esp_http_client_handle_t client,
                         char *buffer,
                         int bufferLength) {
  if (client == nullptr || buffer == nullptr || bufferLength < 0) {
    return -1;
  }
  auto &response = getResponse(client);
  response.readCallCount++;
  if (client->scriptedReadIndex < response.scriptedReads.size()) {
    int scriptedResult = response.scriptedReads[client->scriptedReadIndex++];
    if (scriptedResult <= 0) {
      return scriptedResult;
    }
    bufferLength = std::min(bufferLength, scriptedResult);
  }
  if (bufferLength == 0 || client->offset >= response.body.size()) {
    return 0;
  }

  size_t requested = static_cast<size_t>(bufferLength);
  if (client->chunkIndex < response.chunks.size()) {
    size_t chunkLeft =
        response.chunks[client->chunkIndex] - client->chunkOffset;
    requested = std::min(requested, chunkLeft);
  }
  size_t available = response.body.size() - client->offset;
  size_t copied = std::min(requested, available);
  memcpy(buffer, response.body.data() + client->offset, copied);
  client->offset += copied;

  if (client->chunkIndex < response.chunks.size()) {
    client->chunkOffset += copied;
    if (client->chunkOffset == response.chunks[client->chunkIndex]) {
      client->chunkIndex++;
      client->chunkOffset = 0;
    }
  }
  return static_cast<int>(copied);
}

bool esp_http_client_is_complete_data_received(
    esp_http_client_handle_t client) {
  return client != nullptr && getResponse(client).responseComplete &&
         client->offset == getResponse(client).body.size();
}

int esp_http_client_get_status_code(esp_http_client_handle_t client) {
  return getResponse(client).statusCode;
}

int64_t esp_http_client_get_content_length(esp_http_client_handle_t client) {
  auto &response = getResponse(client);
  if (response.contentLength >= 0) {
    return response.contentLength;
  }
  return static_cast<int64_t>(response.body.size());
}

int esp_http_client_get_errno(esp_http_client_handle_t client) {
  return getResponse(client).errnoCode;
}

esp_err_t esp_http_client_get_and_clear_last_tls_error(
    esp_http_client_handle_t client, int *espTlsCode, int *espTlsFlags) {
  auto &response = getResponse(client);
  if (espTlsCode) {
    *espTlsCode = response.tlsCode;
  }
  if (espTlsFlags) {
    *espTlsFlags = response.tlsFlags;
  }
  return ESP_OK;
}

const esp_partition_t *esp_ota_get_next_update_partition(
    const esp_partition_t *) {
  return &updatePartition;
}

esp_err_t esp_ota_begin(const esp_partition_t *,
                        size_t,
                        esp_ota_handle_t *outHandle) {
  state.otaBeginCount++;
  state.partitionData.clear();
  if (outHandle) {
    *outHandle = 1;
  }
  return ESP_OK;
}

esp_err_t esp_ota_write(esp_ota_handle_t, const void *data, size_t size) {
  auto bytes = static_cast<const uint8_t *>(data);
  state.partitionData.insert(state.partitionData.end(), bytes, bytes + size);
  return ESP_OK;
}

esp_err_t esp_ota_end(esp_ota_handle_t) {
  state.otaEndCount++;
  return state.otaEndResult;
}

esp_err_t esp_ota_abort(esp_ota_handle_t) {
  state.otaAbortCount++;
  return ESP_OK;
}

esp_err_t esp_ota_set_boot_partition(const esp_partition_t *) {
  state.bootPartitionSet = true;
  return ESP_OK;
}

esp_err_t esp_partition_read(const esp_partition_t *,
                             size_t offset,
                             void *destination,
                             size_t size) {
  if (offset + size > state.partitionData.size()) {
    return ESP_FAIL;
  }
  memcpy(destination, state.partitionData.data() + offset, size);
  return ESP_OK;
}

}  // extern "C"

Supla::RsaVerificator::RsaVerificator(const uint8_t *) {
}

Supla::RsaVerificator::~RsaVerificator() {
}

bool Supla::RsaVerificator::verify(Supla::Sha256 *, const uint8_t *) {
  state.rsaVerificationCalled = true;
  return state.rsaVerificationResult;
}
