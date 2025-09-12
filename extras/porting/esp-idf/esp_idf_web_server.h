/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
*/

#ifndef EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_
#define EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_

#include <esp_http_server.h>
#include <supla/network/web_sender.h>
#include <supla/network/web_server.h>
#include <supla/network/html_generator.h>
#include <supla/storage/config.h>

namespace Supla {

class EspIdfSender : public Supla::WebSender {
 public:
  explicit EspIdfSender(httpd_req_t *req);
  ~EspIdfSender();
  void send(const char *, int) override;

 protected:
  httpd_req_t *reqHandler;
  bool error = false;
};

class EspIdfWebServer : public Supla::WebServer {
 public:
  explicit EspIdfWebServer(HtmlGenerator *generator = nullptr);
  virtual ~EspIdfWebServer();
  void start() override;
  void stop() override;

  bool handlePost(httpd_req_t *req, bool beta = false);

  void setServerCertificate(const uint8_t *serverCert,
                            int serverCertLen,
                            const uint8_t *prvtKey,
                            int prvtKeyLen);

  bool dataSaved = false;

  /**
   * @brief Verifies https server certificates format
   *
   * @return true if certificates are in PEM format
   */
  bool verifyCertificatesFormat() override;
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

 protected:
  static uint32_t getIpFromReq(httpd_req_t *req);
  void cleanupCerts();
  bool isSessionCookieValid(const char *sessionCookie);
  void setSessionCookie(httpd_req_t *req, char *buf, int bufLen);
  void failedLoginAttempt(httpd_req_t *req);

  httpd_handle_t serverHttps = {};
  httpd_handle_t serverHttp = {};
  const uint8_t *serverCert = nullptr;
  uint8_t *prvtKey = nullptr;
  uint16_t serverCertLen = 0;
  uint16_t prvtKeyLen = 0;

  uint32_t lastLoginAttemptTimestamp = 0;
  SaltPassword saltPassword = {};
  uint8_t sessionSecret[32] = {};

  uint8_t failedLoginAttempts = 0;
  bool prvtKeyDecrypted = false;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_
