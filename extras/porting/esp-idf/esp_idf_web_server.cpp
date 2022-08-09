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

#include <stddef.h>
#include <string.h>
#include <supla-common/log.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/log_wrapper.h>

#include "esp_idf_web_server.h"
#include "supla/network/html_generator.h"

static Supla::EspIdfWebServer *serverInstance = nullptr;

esp_err_t getFavicon(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get favicon.ico");
  if (serverInstance) {
    serverInstance->notifyClientConnected();
  }
  httpd_resp_set_type(req, "image/x-icon");
  httpd_resp_send(req, (const char *)(Supla::favico), sizeof(Supla::favico));
  return ESP_OK;
}

esp_err_t getHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get request");
  Supla::EspIdfSender sender(req);

  if (serverInstance && serverInstance->htmlGenerator) {
    serverInstance->notifyClientConnected();
    serverInstance->htmlGenerator->sendPage(&sender, serverInstance->dataSaved);
    serverInstance->dataSaved = false;
  }

  return ESP_OK;
}

esp_err_t getBetaHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: get beta request");
  Supla::EspIdfSender sender(req);

  if (serverInstance && serverInstance->htmlGenerator) {
    serverInstance->notifyClientConnected();
    serverInstance->htmlGenerator->sendBetaPage(&sender,
                                                serverInstance->dataSaved);
    serverInstance->dataSaved = false;
  }

  return ESP_OK;
}

esp_err_t postHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: post request");
  if (serverInstance) {
    if (serverInstance->handlePost(req)) {
      httpd_resp_set_status(req, "303 See Other");
      httpd_resp_set_hdr(req, "Location", "/");
      httpd_resp_send(req, "Supla", 5);

      return ESP_OK;
    }
  }
  return ESP_FAIL;
}

esp_err_t postBetaHandler(httpd_req_t *req) {
  SUPLA_LOG_DEBUG("SERVER: post request");
  if (serverInstance) {
    if (serverInstance->handlePost(req)) {
      httpd_resp_set_status(req, "303 See Other");
      httpd_resp_set_hdr(req, "Location", "/beta");
      httpd_resp_send(req, "Supla", 5);

      return ESP_OK;
    }
  }
  return ESP_FAIL;
}

httpd_uri_t uriGet = {
  .uri = "/", .method = HTTP_GET, .handler = getHandler, .user_ctx = NULL};

httpd_uri_t uriGetBeta = {.uri = "/beta",
  .method = HTTP_GET,
  .handler = getBetaHandler,
  .user_ctx = NULL};

httpd_uri_t uriFavicon = {.uri = "/favicon.ico",
                          .method = HTTP_GET,
                          .handler = getFavicon,
                          .user_ctx = NULL};

httpd_uri_t uriPost = {
    .uri = "/", .method = HTTP_POST, .handler = postHandler, .user_ctx = NULL};

httpd_uri_t uriPostBeta = {.uri = "/beta",
                           .method = HTTP_POST,
                           .handler = postBetaHandler,
                           .user_ctx = NULL};

Supla::EspIdfWebServer::EspIdfWebServer(Supla::HtmlGenerator *generator)
    : WebServer(generator) {
  serverInstance = this;
}

Supla::EspIdfWebServer::~EspIdfWebServer() {
  serverInstance = nullptr;
}

bool Supla::EspIdfWebServer::handlePost(httpd_req_t *req) {
  notifyClientConnected();
  resetParser();

  size_t contentLen = req->content_len;
  SUPLA_LOG_DEBUG("SERVER: content length %d B", contentLen);

  char content[500];
  size_t recvSize = req->content_len;
  if (sizeof(content) - 1 < recvSize) {
    recvSize = sizeof(content) - 1;
  }

  int ret = 0;

  while (contentLen > 0) {
    ret = httpd_req_recv(req, content, recvSize);
    content[ret] = '\0';
    SUPLA_LOG_DEBUG(
              "SERVER: received %d B (cl %d): %s",
              ret,
              contentLen,
              content);
    contentLen -= ret;
    if (ret <= 0) {
      contentLen = 0;
      break;
    }

    parsePost(content, ret, (contentLen == 0));

    delay(1);
  }
  if (ret <= 0) {
    resetParser();
    if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
      httpd_resp_send_408(req);
    }
    return false;
  }

  // call getHandler to generate config html page
  serverInstance->dataSaved = true;

  return true;
}

void Supla::EspIdfWebServer::start() {
  if (server) {
    return;
  }

  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.lru_purge_enable = true;

  SUPLA_LOG_INFO("Starting local web server");

  if (httpd_start(&server, &config) == ESP_OK) {
    httpd_register_uri_handler(server, &uriGet);
    httpd_register_uri_handler(server, &uriGetBeta);
    httpd_register_uri_handler(server, &uriFavicon);
    httpd_register_uri_handler(server, &uriPost);
    httpd_register_uri_handler(server, &uriPostBeta);
  }
}

void Supla::EspIdfWebServer::stop() {
  SUPLA_LOG_INFO("Stopping local web server");
  if (server) {
    httpd_stop(server);
    server = nullptr;
  }
}

Supla::EspIdfSender::EspIdfSender(httpd_req_t *req) : reqHandler(req) {
}

Supla::EspIdfSender::~EspIdfSender() {
  if (!error) {
    httpd_resp_send_chunk(reqHandler, nullptr, 0);
  }
}

void Supla::EspIdfSender::send(const char *buf, int size) {
  if (error || !buf || !reqHandler) {
    return;
  }
  if (size == -1) {
    size = strlen(buf);
  }
  if (size == 0) {
    return;
  }

  esp_err_t err = httpd_resp_send_chunk(reqHandler, buf, size);
  if (err != ESP_OK) {
    error = true;
  }
}
