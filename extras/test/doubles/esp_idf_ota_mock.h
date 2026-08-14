// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_OTA_MOCK_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_OTA_MOCK_H_

#include <stddef.h>

#include <string>
#include <vector>

namespace EspIdfOtaMock {

void reset();
void setHttpResponse(const std::string &body,
                     const std::vector<size_t> &chunks = {});
void addHttpResponse(const std::string &body,
                     const std::vector<size_t> &chunks = {});
void setResponseComplete(bool complete);
void setRsaVerificationResult(bool result);
const std::string &getRequestBody();
const std::string &getRequestUrl();
bool wasCertificateConfigured();
size_t getHttpClientInitCount();
size_t getOtaWrittenBytes();
bool wasOtaBeginCalled();
bool wasOtaEndCalled();
bool wasRsaVerificationCalled();
bool wasBootPartitionSet();

}  // namespace EspIdfOtaMock

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_OTA_MOCK_H_
