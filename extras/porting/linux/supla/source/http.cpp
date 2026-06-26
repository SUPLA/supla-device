/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "http.h"

#include <curl/curl.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>

#include <algorithm>
#include <cctype>
#include <cinttypes>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

namespace {

size_t writeResponseBody(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* body = static_cast<std::string*>(userdata);
  body->append(ptr, size * nmemb);
  return size * nmemb;
}

std::string trim(std::string value) {
  auto notSpace = [](unsigned char c) { return !std::isspace(c); };
  value.erase(value.begin(),
              std::find_if(value.begin(), value.end(), notSpace));
  value.erase(std::find_if(value.rbegin(), value.rend(), notSpace).base(),
              value.end());
  return value;
}

bool isSuccessStatus(int64_t statusCode) {
  return statusCode >= 200 && statusCode < 300;
}

long curlLong(unsigned int value) {  // NOLINT(runtime/int)
  return static_cast<long>(value);  // NOLINT(runtime/int)
}

}  // namespace

namespace Supla::Source {

HttpResponse CurlHttpTransport::perform(const HttpRequest& request) {
  HttpResponse response;

  CURL* curl = curl_easy_init();
  if (!curl) {
    response.error = "curl initialization failed";
    return response;
  }

  curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                   curlLong(request.timeoutMs));
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, curlLong(request.timeoutMs));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeResponseBody);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response.body);
  curl_easy_setopt(curl, CURLOPT_USERAGENT, "supla-device-sd4linux/1.0");

  if (request.method == "GET") {
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
  } else {
    response.error = "unsupported method";
    curl_easy_cleanup(curl);
    return response;
  }

  curl_slist* headerList = nullptr;
  for (const auto& header : request.headers) {
    std::string entry = header.first + ": " + header.second;
    headerList = curl_slist_append(headerList, entry.c_str());
  }
  if (headerList) {
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
  }

  char errorBuffer[CURL_ERROR_SIZE] = {};
  curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errorBuffer);

  CURLcode result = curl_easy_perform(curl);
  long responseCode = 0;  // NOLINT(runtime/int)
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &responseCode);
  response.statusCode = responseCode;

  if (result == CURLE_OK) {
    response.transportOk = true;
  } else if (errorBuffer[0] != '\0') {
    response.error = errorBuffer;
  } else {
    response.error = curl_easy_strerror(result);
  }

  if (headerList) {
    curl_slist_free_all(headerList);
  }
  curl_easy_cleanup(curl);

  return response;
}

Http::Http(const std::string& method,
           const std::string& url,
           const std::map<std::string, std::string>& headers,
           const std::string& authType,
           const std::string& tokenFile,
           unsigned int refreshTimeMs,
           unsigned int timeoutMs,
           unsigned int expirationTimeSec,
           std::unique_ptr<HttpTransport> transport)
    : method(method),
      url(url),
      headers(headers),
      authType(authType),
      tokenFile(tokenFile),
      refreshTimeMs(refreshTimeMs),
      timeoutMs(timeoutMs),
      expirationTimeSec(expirationTimeSec),
      transport(std::move(transport)) {
}

Http::~Http() {
  if (worker.joinable()) {
    worker.join();
  }
}

std::string Http::getContent() {
  joinFinishedWorker();

  uint32_t now = millis();
  if (shouldFetch(now)) {
    startFetch(now);
  }

  std::lock_guard<std::mutex> lock(mutex);
  return cachedBody;
}

bool Http::isConnected() {
  joinFinishedWorker();

  std::lock_guard<std::mutex> lock(mutex);
  if (!hasAttempted) {
    return true;
  }

  if (!hasSuccessfulResponse) {
    return false;
  }

  return !hasExpired(millis());
}

bool Http::shouldFetch(uint32_t now) const {
  std::lock_guard<std::mutex> lock(mutex);
  if (fetchInProgress) {
    return false;
  }
  if (!hasAttempted) {
    return true;
  }
  return now - lastAttemptMs >= refreshTimeMs;
}

void Http::joinFinishedWorker() {
  if (!worker.joinable()) {
    return;
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    if (fetchInProgress) {
      return;
    }
  }

  worker.join();
}

void Http::startFetch(uint32_t now) {
  joinFinishedWorker();

  HttpRequest request;
  request.method = method;
  request.url = url;
  request.headers = headers;
  request.timeoutMs = timeoutMs;

  if (authType == "bearer_file") {
    std::string token;
    if (!readBearerToken(&token)) {
      std::lock_guard<std::mutex> lock(mutex);
      lastAttemptMs = now;
      hasAttempted = true;
      return;
    }
    request.headers["Authorization"] = "Bearer " + token;
  }

  {
    std::lock_guard<std::mutex> lock(mutex);
    lastAttemptMs = now;
    hasAttempted = true;
    fetchInProgress = true;
  }

  worker = std::thread(&Http::fetchWorker, this, request, now);
}

void Http::fetchWorker(HttpRequest request, uint32_t attemptMs) {
  SUPLA_LOG_DEBUG(
      "HTTP Source: %s %s", request.method.c_str(), request.url.c_str());
  HttpResponse response = transport->perform(request);
  uint32_t durationMs = millis() - attemptMs;
  if (response.transportOk && isSuccessStatus(response.statusCode)) {
    SUPLA_LOG_DEBUG("HTTP Source: status %" PRId64 ", duration %u ms",
                    response.statusCode,
                    durationMs);
    std::lock_guard<std::mutex> lock(mutex);
    cachedBody = response.body;
    lastSuccessMs = millis();
    hasSuccessfulResponse = true;
    fetchInProgress = false;
    cacheExpiredLog = false;
    return;
  }

  logFailure(response.statusCode, response.error);
  SUPLA_LOG_DEBUG("HTTP Source: failed after %u ms", durationMs);
  {
    std::lock_guard<std::mutex> lock(mutex);
    fetchInProgress = false;
  }
}

bool Http::readBearerToken(std::string* token) const {
  std::ifstream file(tokenFile);
  if (!file.is_open()) {
    SUPLA_LOG_ERROR("HTTP Source: failed to open bearer token file \"%s\"",
                    tokenFile.c_str());
    return false;
  }

  std::ostringstream buffer;
  buffer << file.rdbuf();
  *token = trim(buffer.str());
  if (token->empty()) {
    SUPLA_LOG_ERROR("HTTP Source: bearer token file \"%s\" is empty",
                    tokenFile.c_str());
    return false;
  }

  return true;
}

void Http::logFailure(int64_t statusCode, const std::string& error) {
  if (statusCode == 401 || statusCode == 403) {
    SUPLA_LOG_ERROR("HTTP Source: authorization failed with status %" PRId64,
                    statusCode);
  } else if (statusCode == 404) {
    SUPLA_LOG_ERROR("HTTP Source: resource not found, status 404");
  } else if (statusCode == 429) {
    SUPLA_LOG_WARNING("HTTP Source: rate limited, status 429");
  } else if (statusCode >= 500) {
    SUPLA_LOG_WARNING("HTTP Source: server error, status %" PRId64,
                      statusCode);
  } else if (statusCode > 0) {
    SUPLA_LOG_WARNING("HTTP Source: request failed with status %" PRId64,
                      statusCode);
  } else {
    SUPLA_LOG_WARNING("HTTP Source: request failed: %s", error.c_str());
  }
}

bool Http::hasExpired(uint32_t now) {
  if (expirationTimeSec == 0) {
    return false;
  }

  bool expired = now - lastSuccessMs >= expirationTimeSec * 1000u;
  if (expired && !cacheExpiredLog) {
    SUPLA_LOG_DEBUG("HTTP Source: cached response expired");
    cacheExpiredLog = true;
  }
  return expired;
}

}  // namespace Supla::Source
