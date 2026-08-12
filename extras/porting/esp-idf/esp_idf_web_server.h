// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_

#include <esp_http_server.h>
#include <supla/network/html_output_buffer.h>
#include <supla/network/web_sender.h>
#include <supla/network/web_server.h>
#include <supla/network/html_generator.h>
#include <supla/storage/config.h>

namespace Supla {

class EspIdfSender : public Supla::WebSender {
 public:
  explicit EspIdfSender(httpd_req_t *req, char *sendBuf, int sendBufLen);
  ~EspIdfSender();
  void send(const char *, int) override;

 protected:
  static bool flushChunk(void *context, const char *buf, int size);
  httpd_req_t *reqHandler;
  HtmlOutputBuffer outputBuffer;
};

class EspIdfWebServer : public Supla::WebServer {
 public:
  enum class PostRequestResult {
    OK,
    TIMEOUT,
    INVALID_REQUEST,
    CSRF_INVALID,
  };

  explicit EspIdfWebServer(HtmlGenerator *generator = nullptr,
                           WebServerMode mode = WebServerMode::Auto);
  virtual ~EspIdfWebServer();
  void start() override;
  void stop() override;
  void setWebServerMode(WebServerMode mode) override;
  WebServerMode getWebServerMode() const override;
  WebServerMode resolveWebServerMode() const override;

  PostRequestResult handlePost(httpd_req_t *req, bool beta = false);

  // Stores pointers to the embedded HTTPS certificate material provided by
  // the board code. The web server keeps a separate active runtime copy.
  void setServerCertificate(const uint8_t *serverCert,
                            int serverCertLen,
                            const uint8_t *prvtKey,
                            int prvtKeyLen);

  bool dataSaved = false;

  /**
   * @brief Verifies embedded HTTPS server certificates format
   *
   * @return true if certificates are valid
   * @return false otherwise
   */
  bool verifyEmbeddedHttpsCertificates() override;
  bool ensureAuthorized(httpd_req_t *req,
                        char *sessionCookie,
                        int sessionCookieLen,
                        bool loginFailed = false);
  void renderLoginPage(httpd_req_t *req);
  esp_err_t redirect(httpd_req_t *req,
                int code,
                const char *destination,
                const char *cookieRedirect = nullptr);
  const char *loginOrSetupUrl() const;

  bool login(httpd_req_t *req,
             const char *password,
             char *sessionCookie,
             int sessionCookieLen);
  void handleLogout(httpd_req_t *req);
  SetupRequestResult handleSetup(httpd_req_t *req,
                                 char *sessionCookie,
                                 int sessionCookieLen);

  bool isPasswordConfigured() const;
  bool isPasswordCorrect(const char *password) const;
  bool isHttpsEnalbled() const;
  bool isAuthorizationBlocked();
  void reloadSaltPassword();
  void addSecurityLog(httpd_req_t *req, const char *log) const;

  char *getSendBufPtr() const;

 protected:
  static uint32_t getIpFromReq(httpd_req_t *req);
  // Clears only the runtime HTTPS cert copy owned by the web server. The
  // embedded source pointers passed from board code are borrowed and must stay
  // untouched.
  void cleanupCerts();
  bool setActiveCertificateBuffers(uint8_t *serverCert,
                                   int serverCertLen,
                                   uint8_t *prvtKey,
                                   int prvtKeyLen);
  bool setActivePrivateKeyBuffer(uint8_t *prvtKey, int prvtKeyLen);
  bool isSessionCookieValid(const char *sessionCookie);
  void setSessionCookie(httpd_req_t *req, char *buf, int bufLen);
  void failedLoginAttempt(httpd_req_t *req);
  bool loadEmbeddedHttpsCertificates(bool storeActive = true);
  bool ensureHttpsCertificates();
  bool loadHttpsCertificatesFromStorage();
  bool generateHttpsCertificates();

  httpd_handle_t serverHttps = {};
  httpd_handle_t serverHttp = {};
  // Borrowed pointers to the embedded cert/key material provided by board
  // code. The web server never owns or frees them.
  const uint8_t *embeddedServerCert = nullptr;
  const uint8_t *embeddedPrvtKey = nullptr;
  uint16_t embeddedServerCertLen = 0;
  uint16_t embeddedPrvtKeyLen = 0;
  // Runtime HTTPS cert/key owned by the web server. Embedded PEM certificates
  // can leave these null and are passed directly to esp_https_server at start.
  uint8_t *serverCert = nullptr;
  uint8_t *prvtKey = nullptr;
  uint16_t serverCertLen = 0;
  uint16_t prvtKeyLen = 0;
  WebServerMode webServerMode = WebServerMode::Auto;

  uint32_t lastLoginAttemptTimestamp = 0;
  SaltPassword saltPassword = {};
  uint8_t sessionSecret[32] = {};

  uint8_t failedLoginAttempts = 0;
  char *sendBuf = nullptr;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_
