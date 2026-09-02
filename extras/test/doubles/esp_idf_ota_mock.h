// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_OTA_MOCK_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_OTA_MOCK_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

namespace EspIdfOtaMock {

void reset();
void setHttpResponse(const std::string &body,
                     const std::vector<size_t> &chunks = {});
void addHttpResponse(const std::string &body,
                     const std::vector<size_t> &chunks = {});
void setHttpReadResults(size_t responseIndex,
                        const std::vector<int> &results);
void setHttpOpenError(size_t responseIndex,
                      int tlsCode = 0,
                      int tlsFlags = 0);
void setHttpHeadersError(size_t responseIndex,
                         int tlsCode = 0,
                         int tlsFlags = 0);
void setHttpStatusCode(size_t responseIndex, int statusCode);
void setHttpContentLength(size_t responseIndex, int64_t contentLength);
void setResponseComplete(bool complete);
void setResponseComplete(size_t responseIndex, bool complete);
void setOtaEndResult(int result);
void setRsaVerificationResult(bool result);
const std::string &getRequestBody();
const std::string &getRequestUrl();
bool wasCertificateConfigured();
size_t getHttpClientInitCount();
size_t getHttpReadCallCount(size_t responseIndex);
size_t getOtaWrittenBytes();
size_t getOtaBeginCount();
size_t getOtaEndCount();
size_t getOtaAbortCount();
bool wasOtaBeginCalled();
bool wasOtaEndCalled();
bool wasOtaAbortCalled();
bool wasRsaVerificationCalled();
bool wasBootPartitionSet();

}  // namespace EspIdfOtaMock

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_OTA_MOCK_H_
