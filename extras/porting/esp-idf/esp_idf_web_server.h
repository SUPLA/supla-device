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

  void setServerCertificate(const char *serverCert,
                            int serverCertLen,
                            const char *prvtKey,
                            int prvtKeyLen);

  bool dataSaved = false;

  /**
   * @brief Verifies https server certificates format
   *
   * @return true if certificates are in PEM format
   */
  bool verfiyCertificatesFormat() override;

 protected:
  void cleanupCerts();
  httpd_handle_t serverHttps = {};
  httpd_handle_t serverHttp = {};
  uint8_t *serverCert = nullptr;
  uint8_t *prvtKey = nullptr;
  int serverCertLen = 0;
  int prvtKeyLen = 0;
  bool prvtKeyDecrypted = false;
};

};  // namespace Supla

#endif  // EXTRAS_PORTING_ESP_IDF_ESP_IDF_WEB_SERVER_H_
