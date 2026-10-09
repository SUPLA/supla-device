// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <arpa/inet.h>
#include <poll.h>
#include <sys/socket.h>
#include <unistd.h>
#include <supla/source/http.h>

#include <atomic>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

class LocalHttpServer {
 public:
  explicit LocalHttpServer(
      std::function<std::string(const std::string&)> respond)
      : respond(std::move(respond)) {
    socketFd = ::socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    socklen_t size = sizeof(address);
    if (socketFd < 0 ||
        ::bind(socketFd, reinterpret_cast<sockaddr*>(&address), size) != 0 ||
        ::listen(socketFd, 8) != 0 ||
        ::getsockname(socketFd, reinterpret_cast<sockaddr*>(&address),
                      &size) != 0) {
      if (socketFd >= 0) {
        ::close(socketFd);
      }
      throw std::runtime_error("Cannot start local HTTP test server");
    }
    port = ntohs(address.sin_port);
    worker = std::thread([this]() { serve(); });
  }

  ~LocalHttpServer() {
    stopping = true;
    ::shutdown(socketFd, SHUT_RDWR);
    worker.join();
    ::close(socketFd);
  }

  std::string url(const std::string& path = "/",
                  const std::string& host = "127.0.0.1") const {
    return "http://" + host + ":" + std::to_string(port) + path;
  }

  std::vector<std::string> received() {
    std::lock_guard<std::mutex> lock(mutex);
    return requests;
  }

 private:
  void serve() {
    while (!stopping) {
      pollfd descriptor = {socketFd, POLLIN, 0};
      if (::poll(&descriptor, 1, 100) <= 0 || stopping) {
        continue;
      }
      const int client = ::accept(socketFd, nullptr, nullptr);
      if (client < 0) {
        continue;
      }
      timeval timeout = {1, 0};
      ::setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
      std::string request;
      char buffer[1024];
      while (request.find("\r\n\r\n") == std::string::npos &&
             request.size() < 16384) {
        const auto size = ::recv(client, buffer, sizeof(buffer), 0);
        if (size <= 0) {
          break;
        }
        request.append(buffer, static_cast<size_t>(size));
      }
      {
        std::lock_guard<std::mutex> lock(mutex);
        requests.push_back(request);
      }
      const auto response = respond(request);
      size_t written = 0;
      while (written < response.size()) {
        const auto size = ::send(client, response.data() + written,
                                 response.size() - written, MSG_NOSIGNAL);
        if (size <= 0) {
          break;
        }
        written += static_cast<size_t>(size);
      }
      ::close(client);
    }
  }

  int socketFd = -1;
  uint16_t port = 0;
  std::function<std::string(const std::string&)> respond;
  std::atomic<bool> stopping{false};
  std::thread worker;
  std::mutex mutex;
  std::vector<std::string> requests;
};

std::string redirect(const std::string& target) {
  return "HTTP/1.1 302 Found\r\nLocation: " + target +
         "\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
}

std::string ok(const std::string&) {
  return "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n"
         "Connection: close\r\n\r\nok";
}

}  // namespace

TEST(Sd4linuxHttpRedirectTests, HeadersDisableRedirectsToAnotherServer) {
  LocalHttpServer target(ok);
  LocalHttpServer origin([&](const std::string&) {
    return redirect(target.url("/", "localhost"));
  });
  Supla::Source::CurlHttpTransport transport;
  for (const auto& name : {"X-API-Key", "Authorization", "Accept"}) {
    Supla::Source::HttpRequest request;
    request.url = origin.url();
    request.headers[name] = "dummy-test-value";
    const auto response = transport.perform(request);
    EXPECT_TRUE(response.transportOk);
    EXPECT_EQ(response.statusCode, 302);
    EXPECT_TRUE(target.received().empty());
  }
  const auto requests = origin.received();
  ASSERT_EQ(requests.size(), 3U);
  EXPECT_NE(requests[0].find("X-API-Key: dummy-test-value"), std::string::npos);
}

TEST(Sd4linuxHttpRedirectTests, HeadersDisableSameServerRedirects) {
  LocalHttpServer server([](const std::string& request) {
    return request.find("GET /final ") == 0 ? ok(request) : redirect("/final");
  });
  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.url = server.url();
  request.headers["X-API-Key"] = "dummy-test-value";
  const auto response = transport.perform(request);
  EXPECT_TRUE(response.transportOk);
  EXPECT_EQ(response.statusCode, 302);
  EXPECT_EQ(server.received().size(), 1U);
}

TEST(Sd4linuxHttpRedirectTests, FollowsRedirectWithoutHeaders) {
  LocalHttpServer target(ok);
  LocalHttpServer origin([&](const std::string&) {
    return redirect(target.url("/", "localhost"));
  });
  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.url = origin.url();
  const auto response = transport.perform(request);
  EXPECT_TRUE(response.transportOk);
  EXPECT_EQ(response.statusCode, 200);
  EXPECT_EQ(response.body, "ok");
  EXPECT_EQ(origin.received().size(), 1U);
  EXPECT_EQ(target.received().size(), 1U);
}

TEST(Sd4linuxHttpRedirectTests, StopsRedirectLoopAfterFiveRedirects) {
  LocalHttpServer server([](const std::string&) { return redirect("/"); });
  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.url = server.url();
  const auto response = transport.perform(request);
  EXPECT_FALSE(response.transportOk);
  EXPECT_FALSE(response.error.empty());
  EXPECT_EQ(server.received().size(), 6U);
}

TEST(Sd4linuxHttpRedirectTests, AllowsExactlyFiveRedirects) {
  LocalHttpServer server([](const std::string& request) {
    const int index = request.at(5) - '0';
    return index == 5 ? ok(request) : redirect("/" + std::to_string(index + 1));
  });
  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.url = server.url("/0");
  const auto response = transport.perform(request);
  EXPECT_TRUE(response.transportOk);
  EXPECT_EQ(response.statusCode, 200);
  EXPECT_EQ(response.body, "ok");
  EXPECT_EQ(server.received().size(), 6U);
}

TEST(Sd4linuxHttpRedirectTests, RejectsFtpRedirectBeforeContactingTarget) {
  LocalHttpServer target(ok);
  LocalHttpServer origin([&](const std::string&) {
    auto url = target.url();
    url.replace(0, 4, "ftp");
    return redirect(url);
  });
  Supla::Source::CurlHttpTransport transport;
  Supla::Source::HttpRequest request;
  request.url = origin.url();
  request.timeoutMs = 1000;
  const auto response = transport.perform(request);
  EXPECT_FALSE(response.transportOk);
  EXPECT_FALSE(response.error.empty());
  EXPECT_TRUE(target.received().empty());
}
