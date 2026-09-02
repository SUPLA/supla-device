// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_SOURCE_HTTP_H_
#define EXTRAS_PORTING_LINUX_SUPLA_SOURCE_HTTP_H_

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "source.h"

namespace Supla::Source {

constexpr unsigned int HTTP_SOURCE_DEFAULT_MAX_BODY_SIZE_BYTES = 1024 * 1024;

struct HttpRequest {
  std::string method = "GET";
  std::string url;
  std::map<std::string, std::string> headers;
  unsigned int timeoutMs = 10000;
  unsigned int maxBodySizeBytes = HTTP_SOURCE_DEFAULT_MAX_BODY_SIZE_BYTES;
};

struct HttpResponse {
  bool transportOk = false;
  int64_t statusCode = 0;
  std::string body;
  std::string error;
};

class HttpTransport {
 public:
  virtual ~HttpTransport() {}
  virtual HttpResponse perform(const HttpRequest& request) = 0;
};

class CurlHttpTransport : public HttpTransport {
 public:
  HttpResponse perform(const HttpRequest& request) override;
};

class Http : public Source {
 public:
  Http(const std::string& method,
       const std::string& url,
       const std::map<std::string, std::string>& headers,
       const std::string& authType,
       const std::string& tokenFile,
       unsigned int refreshTimeMs,
       unsigned int timeoutMs,
       unsigned int expirationTimeSec,
       unsigned int maxBodySizeBytes,
       std::unique_ptr<HttpTransport> transport =
           std::unique_ptr<HttpTransport>(new CurlHttpTransport()));
  Http(const std::string& method,
       const std::string& url,
       const std::map<std::string, std::string>& headers,
       const std::string& authType,
       const std::string& tokenFile,
       unsigned int refreshTimeMs,
       unsigned int timeoutMs,
       unsigned int expirationTimeSec,
       std::unique_ptr<HttpTransport> transport =
           std::unique_ptr<HttpTransport>(new CurlHttpTransport()));
  ~Http();

  std::string getContent() override;
  bool isConnected() override;

 private:
  void joinFinishedWorker();
  bool shouldFetch(uint32_t now) const;
  void startFetch(uint32_t now);
  void fetchWorker(HttpRequest request, uint32_t attemptMs);
  bool readBearerToken(std::string* token) const;
  void logFailure(int64_t statusCode, const std::string& error);
  bool hasExpired(uint32_t now);

  std::string method;
  std::string url;
  std::map<std::string, std::string> headers;
  std::string authType;
  std::string tokenFile;
  unsigned int refreshTimeMs = 30000;
  unsigned int timeoutMs = 10000;
  unsigned int expirationTimeSec = 10 * 60;
  unsigned int maxBodySizeBytes = HTTP_SOURCE_DEFAULT_MAX_BODY_SIZE_BYTES;
  std::unique_ptr<HttpTransport> transport;
  std::thread worker;
  mutable std::mutex mutex;
  std::string cachedBody;
  uint32_t lastAttemptMs = 0;
  uint32_t lastSuccessMs = 0;
  bool hasAttempted = false;
  bool hasSuccessfulResponse = false;
  bool fetchInProgress = false;
  bool cacheExpiredLog = false;
};

}  // namespace Supla::Source

#endif  // EXTRAS_PORTING_LINUX_SUPLA_SOURCE_HTTP_H_
