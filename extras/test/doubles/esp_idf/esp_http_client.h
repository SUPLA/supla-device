// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_HTTP_CLIENT_H_
#define EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_HTTP_CLIENT_H_

#include <stdbool.h>
#include <stdint.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct esp_http_client *esp_http_client_handle_t;

typedef enum {
  HTTP_METHOD_GET = 0,
  HTTP_METHOD_POST,
} esp_http_client_method_t;

typedef struct {
  const char *url;
  int timeout_ms;
  const char *user_agent;
  const char *cert_pem;
} esp_http_client_config_t;

esp_http_client_handle_t esp_http_client_init(
    const esp_http_client_config_t *config);
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t client);
esp_err_t esp_http_client_set_method(esp_http_client_handle_t client,
                                     esp_http_client_method_t method);
esp_err_t esp_http_client_set_header(esp_http_client_handle_t client,
                                     const char *key,
                                     const char *value);
esp_err_t esp_http_client_open(esp_http_client_handle_t client,
                               int writeLength);
int esp_http_client_write(esp_http_client_handle_t client,
                          const char *data,
                          int length);
int64_t esp_http_client_fetch_headers(esp_http_client_handle_t client);
int esp_http_client_read(esp_http_client_handle_t client,
                         char *buffer,
                         int bufferLength);
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t client);
int esp_http_client_get_status_code(esp_http_client_handle_t client);
int64_t esp_http_client_get_content_length(esp_http_client_handle_t client);
int esp_http_client_get_errno(esp_http_client_handle_t client);
esp_err_t esp_http_client_get_and_clear_last_tls_error(
    esp_http_client_handle_t client, int *espTlsCode, int *espTlsFlags);

#ifdef __cplusplus
}
#endif

#endif  // EXTRAS_TEST_DOUBLES_ESP_IDF_ESP_HTTP_CLIENT_H_
