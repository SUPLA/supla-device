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

#include <gtest/gtest.h>

#include <chrono>
#include <cstdio>
#include <cstdint>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <simple_time.h>
#include <supla/source/http.h>

namespace {

class FakeHttpTransport : public Supla::Source::HttpTransport {
 public:
  Supla::Source::HttpResponse perform(
      const Supla::Source::HttpRequest& request) override {
    std::lock_guard<std::mutex> lock(mutex);
    requests.push_back(request);
    if (responses.empty()) {
      Supla::Source::HttpResponse response;
      response.error = "no response configured";
      return response;
    }
    auto response = responses.front();
    responses.erase(responses.begin());
    return response;
  }

  void pushResponse(const Supla::Source::HttpResponse& response) {
    std::lock_guard<std::mutex> lock(mutex);
    responses.push_back(response);
  }

  size_t requestCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return requests.size();
  }

  Supla::Source::HttpRequest requestAt(size_t index) const {
    std::lock_guard<std::mutex> lock(mutex);
    return requests.at(index);
  }

  mutable std::mutex mutex;
  std::vector<Supla::Source::HttpRequest> requests;
  std::vector<Supla::Source::HttpResponse> responses;
};

class BlockingHttpTransport : public Supla::Source::HttpTransport {
 public:
  Supla::Source::HttpResponse perform(
      const Supla::Source::HttpRequest& request) override {
    {
      std::lock_guard<std::mutex> lock(mutex);
      requests.push_back(request);
    }

    while (true) {
      {
        std::lock_guard<std::mutex> lock(mutex);
        if (releaseCount > 0) {
          releaseCount--;
          break;
        }
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    Supla::Source::HttpResponse response;
    response.transportOk = true;
    response.statusCode = 200;
    response.body = "ok";
    return response;
  }

  void releaseOne() {
    std::lock_guard<std::mutex> lock(mutex);
    releaseCount++;
  }

  size_t requestCount() const {
    std::lock_guard<std::mutex> lock(mutex);
    return requests.size();
  }

  mutable std::mutex mutex;
  std::vector<Supla::Source::HttpRequest> requests;
  int releaseCount = 0;
};

Supla::Source::HttpResponse success(int64_t statusCode,
                                    const std::string& body) {
  Supla::Source::HttpResponse response;
  response.transportOk = true;
  response.statusCode = statusCode;
  response.body = body;
  return response;
}

Supla::Source::HttpResponse failure(const std::string& error) {
  Supla::Source::HttpResponse response;
  response.error = error;
  return response;
}

std::unique_ptr<FakeHttpTransport> makeTransport(
    FakeHttpTransport** transportPtr) {
  auto transport = std::unique_ptr<FakeHttpTransport>(new FakeHttpTransport);
  *transportPtr = transport.get();
  return transport;
}

std::unique_ptr<Supla::Source::Http> makeSource(
    FakeHttpTransport** transportPtr,
    unsigned int refreshTimeMs = 1000,
    unsigned int expirationTimeSec = 10,
    const std::string& authType = "none",
    const std::string& tokenFile = "",
    unsigned int maxBodySizeBytes =
        Supla::Source::HTTP_SOURCE_DEFAULT_MAX_BODY_SIZE_BYTES) {
  return std::unique_ptr<Supla::Source::Http>(new Supla::Source::Http(
      "GET",
      "https://example.test/status",
      {{"Accept", "application/json"}},
      authType,
      tokenFile,
      refreshTimeMs,
      5000,
      expirationTimeSec,
      maxBodySizeBytes,
      makeTransport(transportPtr)));
}

void waitForRequests(FakeHttpTransport* transport, size_t expectedCount) {
  for (int i = 0; i < 100; i++) {
    if (transport->requestCount() >= expectedCount) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

template <typename Transport>
void waitForRequests(Transport* transport, size_t expectedCount) {
  for (int i = 0; i < 100; i++) {
    if (transport->requestCount() >= expectedCount) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void waitForContent(Supla::Source::Http* source,
                    const std::string& expectedContent) {
  for (int i = 0; i < 100; i++) {
    if (source->getContent() == expectedContent) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

void waitForConnected(Supla::Source::Http* source, bool expectedConnected) {
  for (int i = 0; i < 100; i++) {
    if (source->isConnected() == expectedConnected) {
      return;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

}  // namespace

TEST(Sd4linuxHttpSourceTests, PerformsRequestAndReturnsBody) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);
  transport->pushResponse(success(200, R"({"state":"ok"})"));

  (void)source->getContent();
  waitForContent(source.get(), R"({"state":"ok"})");
  EXPECT_EQ(source->getContent(), R"({"state":"ok"})");
  ASSERT_EQ(transport->requestCount(), 1u);
  auto request = transport->requestAt(0);
  EXPECT_EQ(request.method, "GET");
  EXPECT_EQ(request.url, "https://example.test/status");
  EXPECT_EQ(request.headers.at("Accept"), "application/json");
  EXPECT_EQ(request.maxBodySizeBytes,
            Supla::Source::HTTP_SOURCE_DEFAULT_MAX_BODY_SIZE_BYTES);
  EXPECT_TRUE(source->isConnected());
}

TEST(Sd4linuxHttpSourceTests, ReportsConnectedBeforeFirstAttempt) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);

  EXPECT_TRUE(source->isConnected());
}

TEST(Sd4linuxHttpSourceTests, UsesCacheUntilRefreshTimeExpires) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);
  transport->pushResponse(success(200, "first"));
  transport->pushResponse(success(200, "second"));

  (void)source->getContent();
  waitForContent(source.get(), "first");
  EXPECT_EQ(source->getContent(), "first");
  time.advance(999);
  EXPECT_EQ(source->getContent(), "first");
  EXPECT_EQ(transport->requestCount(), 1u);

  time.advance(1);
  (void)source->getContent();
  waitForContent(source.get(), "second");
  EXPECT_EQ(source->getContent(), "second");
  EXPECT_EQ(transport->requestCount(), 2u);
}

TEST(Sd4linuxHttpSourceTests, KeepsCachedBodyOnHttpFailure) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);
  transport->pushResponse(success(200, "last-good"));
  transport->pushResponse(success(500, "bad"));

  (void)source->getContent();
  waitForContent(source.get(), "last-good");
  EXPECT_EQ(source->getContent(), "last-good");
  time.advance(1000);
  EXPECT_EQ(source->getContent(), "last-good");
  waitForRequests(transport, 2);
  EXPECT_EQ(source->getContent(), "last-good");
  EXPECT_TRUE(source->isConnected());
  EXPECT_EQ(transport->requestCount(), 2u);
}

TEST(Sd4linuxHttpSourceTests, KeepsCachedBodyOnNetworkFailure) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);
  transport->pushResponse(success(200, "last-good"));
  transport->pushResponse(failure("timeout"));

  (void)source->getContent();
  waitForContent(source.get(), "last-good");
  EXPECT_EQ(source->getContent(), "last-good");
  time.advance(1000);
  EXPECT_EQ(source->getContent(), "last-good");
  waitForRequests(transport, 2);
  EXPECT_EQ(source->getContent(), "last-good");
  EXPECT_TRUE(source->isConnected());
  EXPECT_EQ(transport->requestCount(), 2u);
}

TEST(Sd4linuxHttpSourceTests, KeepsCachedBodyOnTooLargeResponseFailure) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);
  transport->pushResponse(success(200, "last-good"));
  transport->pushResponse(failure("response body too large"));

  (void)source->getContent();
  waitForContent(source.get(), "last-good");
  EXPECT_EQ(source->getContent(), "last-good");
  time.advance(1000);
  EXPECT_EQ(source->getContent(), "last-good");
  waitForRequests(transport, 2);
  EXPECT_EQ(source->getContent(), "last-good");
  EXPECT_TRUE(source->isConnected());
}

TEST(Sd4linuxHttpSourceTests, PassesConfiguredMaxBodySizeToTransport) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport, 1000, 10, "none", "", 123);
  transport->pushResponse(success(200, "ok"));

  (void)source->getContent();
  waitForContent(source.get(), "ok");

  ASSERT_EQ(transport->requestCount(), 1u);
  EXPECT_EQ(transport->requestAt(0).maxBodySizeBytes, 123u);
}

TEST(Sd4linuxHttpSourceTests, ExpiresCachedBodyForConnectionStateOnly) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport, 5000, 1);
  transport->pushResponse(success(200, "last-good"));

  (void)source->getContent();
  waitForContent(source.get(), "last-good");
  EXPECT_EQ(source->getContent(), "last-good");
  EXPECT_TRUE(source->isConnected());

  time.advance(1000);
  EXPECT_EQ(source->getContent(), "last-good");
  EXPECT_FALSE(source->isConnected());
}

TEST(Sd4linuxHttpSourceTests, RetriesFromConnectionCheckAfterCacheExpires) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport, 1000, 1);
  transport->pushResponse(success(200, "first"));
  transport->pushResponse(success(200, "second"));

  (void)source->getContent();
  waitForContent(source.get(), "first");
  EXPECT_EQ(source->getContent(), "first");

  time.advance(1000);
  EXPECT_FALSE(source->isConnected());
  waitForContent(source.get(), "second");
  EXPECT_EQ(transport->requestCount(), 2u);
  EXPECT_EQ(source->getContent(), "second");
  EXPECT_TRUE(source->isConnected());
}

TEST(Sd4linuxHttpSourceTests, SendsTrimmedBearerTokenFromFile) {
  SimpleTime time;
  const std::string tokenPath = "/tmp/sd4linux_http_source_token.txt";
  {
    std::ofstream tokenFile(tokenPath);
    tokenFile << "  test-token\n";
  }

  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(
      &transport, 1000, 10, "bearer_file", tokenPath);
  transport->pushResponse(success(200, "ok"));

  (void)source->getContent();
  waitForContent(source.get(), "ok");
  EXPECT_EQ(source->getContent(), "ok");
  ASSERT_EQ(transport->requestCount(), 1u);
  EXPECT_EQ(transport->requestAt(0).headers.at("Authorization"),
            "Bearer test-token");

  std::remove(tokenPath.c_str());
}

TEST(Sd4linuxHttpSourceTests, DoesNotCallTransportWhenTokenFileIsMissing) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(
      &transport,
      1000,
      10,
      "bearer_file",
      "/tmp/sd4linux_http_source_missing_token_file");

  EXPECT_EQ(source->getContent(), "");
  EXPECT_FALSE(source->isConnected());
  EXPECT_EQ(transport->requestCount(), 0u);
}

TEST(Sd4linuxHttpSourceTests, ReportsOfflineAfterFailedFirstAttempt) {
  SimpleTime time;
  FakeHttpTransport* transport = nullptr;
  auto source = makeSource(&transport);
  transport->pushResponse(failure("timeout"));

  EXPECT_EQ(source->getContent(), "");
  waitForConnected(source.get(), false);
  EXPECT_FALSE(source->isConnected());
}

TEST(Sd4linuxHttpSourceTests, StartsNewRequestAfterLongRunningRequestFinishes) {
  SimpleTime time;
  auto transport = std::unique_ptr<BlockingHttpTransport>(
      new BlockingHttpTransport);
  auto* transportPtr = transport.get();
  Supla::Source::Http source(
      "GET",
      "https://example.test/status",
      {},
      "none",
      "",
      10,
      5000,
      10,
      std::move(transport));

  EXPECT_EQ(source.getContent(), "");
  waitForRequests(transportPtr, 1);

  time.advance(11);
  EXPECT_EQ(source.getContent(), "");
  EXPECT_EQ(transportPtr->requestCount(), 1u);

  transportPtr->releaseOne();
  for (int i = 0; i < 100 && source.getContent() != "ok"; i++) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }

  time.advance(11);
  EXPECT_EQ(source.getContent(), "ok");
  waitForRequests(transportPtr, 2);
  EXPECT_EQ(transportPtr->requestCount(), 2u);

  transportPtr->releaseOne();
}

TEST(Sd4linuxHttpSourceTests, CurlTransportRejectsBodyLargerThanLimit) {
  const std::string bodyPath = "/tmp/sd4linux_http_source_body_limit.txt";
  {
    std::ofstream bodyFile(bodyPath);
    bodyFile << "abcdef";
  }

  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.method = "GET";
  request.url = "file://" + bodyPath;
  request.timeoutMs = 5000;
  request.maxBodySizeBytes = 3;

  auto response = transport.perform(request);

  EXPECT_FALSE(response.transportOk);
  EXPECT_EQ(response.body, "");
  EXPECT_EQ(response.error, "response body too large");

  std::remove(bodyPath.c_str());
}

TEST(Sd4linuxHttpSourceTests, CurlTransportAcceptsBodyWithinLimit) {
  const std::string bodyPath = "/tmp/sd4linux_http_source_body_ok.txt";
  {
    std::ofstream bodyFile(bodyPath);
    bodyFile << "abcdef";
  }

  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.method = "GET";
  request.url = "file://" + bodyPath;
  request.timeoutMs = 5000;
  request.maxBodySizeBytes = 6;

  auto response = transport.perform(request);

  EXPECT_TRUE(response.transportOk);
  EXPECT_EQ(response.body, "abcdef");

  std::remove(bodyPath.c_str());
}
