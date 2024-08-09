/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "supla_ocr_ic.h"

#include <supla/log_wrapper.h>
#include <supla/io.h>
#include <string.h>
#include <esp_http_client.h>
#include <driver/ledc.h>

// TODO(klew): move it to constructor
#define CAM_PIN_PWDN 32
#define CAM_PIN_RESET -1  // software reset will be performed
#define CAM_PIN_XCLK 0
#define CAM_PIN_SIOD 26
#define CAM_PIN_SIOC 27

#define CAM_PIN_D7 35
#define CAM_PIN_D6 34
#define CAM_PIN_D5 39
#define CAM_PIN_D4 36
#define CAM_PIN_D3 21
#define CAM_PIN_D2 19
#define CAM_PIN_D1 18
#define CAM_PIN_D0 5
#define CAM_PIN_VSYNC 25
#define CAM_PIN_HREF 23
#define CAM_PIN_PCLK 22

using Supla::OcrIc;

OcrIc::OcrIc(int ledGpio): ledGpio(ledGpio) {
}

void OcrIc::onInit() {
  // Camera init
  if (CAM_PIN_PWDN != -1) {
    Supla::Io::pinMode(CAM_PIN_PWDN, OUTPUT);
    Supla::Io::digitalWrite(CAM_PIN_PWDN, LOW);
  }

  camera_config_t cameraConfig = {};
  cameraConfig.pin_pwdn  = CAM_PIN_PWDN;
  cameraConfig.pin_reset = CAM_PIN_RESET;
  cameraConfig.pin_xclk = CAM_PIN_XCLK;
  cameraConfig.pin_sccb_sda = CAM_PIN_SIOD;
  cameraConfig.pin_sccb_scl = CAM_PIN_SIOC;

  cameraConfig.pin_d7 = CAM_PIN_D7;
  cameraConfig.pin_d6 = CAM_PIN_D6;
  cameraConfig.pin_d5 = CAM_PIN_D5;
  cameraConfig.pin_d4 = CAM_PIN_D4;
  cameraConfig.pin_d3 = CAM_PIN_D3;
  cameraConfig.pin_d2 = CAM_PIN_D2;
  cameraConfig.pin_d1 = CAM_PIN_D1;
  cameraConfig.pin_d0 = CAM_PIN_D0;
  cameraConfig.pin_vsync = CAM_PIN_VSYNC;
  cameraConfig.pin_href = CAM_PIN_HREF;
  cameraConfig.pin_pclk = CAM_PIN_PCLK;

  // EXPERIMENTAL: Set to 16MHz on ESP32-S2 or ESP32-S3 to enable EDMA mode
  cameraConfig.xclk_freq_hz = 20000000;
  cameraConfig.ledc_timer = LEDC_TIMER_0;
  cameraConfig.ledc_channel = LEDC_CHANNEL_0;

  // YUV422,GRAYSCALE,RGB565,JPEG
  cameraConfig.pixel_format = PIXFORMAT_JPEG;
  // QQVGA-UXGA, For ESP32, do not use sizes above QVGA when not JPEG. The
  // performance of the ESP32-S series has improved a lot, but JPEG mode
  // always gives better frame rates.
  cameraConfig.frame_size = FRAMESIZE_VGA;

  // 0-63, for OV series camera sensors, lower number means higher quality
  cameraConfig.jpeg_quality = 22;
  // When jpeg mode is used, if fb_count more than one, the driver will work
  // in continuous mode.
  cameraConfig.fb_count = 1;
  cameraConfig.fb_location = CAMERA_FB_IN_PSRAM;
  // CAMERA_GRAB_LATEST. Sets when buffers should be filled
  cameraConfig.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

  esp_err_t err = esp_camera_init(&cameraConfig);
  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Camera Init Failed");
  } else {
    SUPLA_LOG_INFO("Camera Init OK");
  }

  // LED init
  if (ledGpio != -1) {
    ledc_timer_config_t ledcTimer = {};
    ledcTimer.speed_mode = LEDC_LOW_SPEED_MODE;
    ledcTimer.duty_resolution = LEDC_TIMER_13_BIT;
    ledcTimer.timer_num = LEDC_TIMER_1;
    ledcTimer.freq_hz = 4000;
    ledcTimer.clk_cfg = LEDC_AUTO_CLK;
    auto result = ledc_timer_config(&ledcTimer);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to configure LEDC timer");
      return;
    }

    ledc_channel_config_t ledcChannel = {};
    ledcChannel.speed_mode = LEDC_LOW_SPEED_MODE;
    ledcChannel.channel = LEDC_CHANNEL_1;
    ledcChannel.timer_sel = LEDC_TIMER_1;
    ledcChannel.intr_type = LEDC_INTR_DISABLE;
    ledcChannel.gpio_num = ledGpio;
    ledcChannel.duty = 0;
    ledcChannel.hpoint = 0;
    result = ledc_channel_config(&ledcChannel);
    if (result != ESP_OK) {
      SUPLA_LOG_ERROR("Failed to configure LEDC channel");
      return;
    }
  }
}

bool OcrIc::takePhoto() {
  SUPLA_LOG_INFO("CAMERA: fetching first image...");
  photoDataBuffer = nullptr;
  photoDataSize = 0;
  fb = esp_camera_fb_get();
  if (!fb) {
    SUPLA_LOG_ERROR("Camera Capture Failed");
    return false;
  }
  releasePhoto();
  SUPLA_LOG_INFO("CAMERA: fetching second image...");
  fb = esp_camera_fb_get();
  if (!fb) {
    SUPLA_LOG_ERROR("Camera Capture Failed");
    return false;
  }

  SUPLA_LOG_INFO(" *** IMAGE captured, size: %d *** ", fb->len);
  photoDataBuffer = fb->buf;
  photoDataSize = fb->len;
  return true;
}

void OcrIc::releasePhoto() {
  if (fb) {
    SUPLA_LOG_INFO("CAMERA: releasing image...");
    esp_camera_fb_return(fb);
    fb = nullptr;
  }
}

bool OcrIc::sendPhotoToOcrServer(const char *url,
    const char *authkey,
    char *resultBuffer,
    int resultBufferSize) {
  // TODO(klew): move it to common implementation based on
  // Supla::Network::Client
  bool success = false;

  const char *headerContent =
    "------WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
    "Content-Disposition: form-data; name=\"image\"; "
    "filename=\"image.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";
  const char *closure = "\r\n\r\n------WebKitFormBoundary7MA4YWxkTrZu0gW--\r\n";

  size_t body_size = strlen(headerContent);

  esp_http_client_config_t config = {};
  config.url = url;
  config.event_handler = nullptr;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(
      client, "X-AuthKey", authkey);
  esp_http_client_set_header(
      client,
      "Content-Type",
      "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW");

  int dataToWrite = body_size + photoDataSize + strlen(closure);
  esp_err_t err =
    esp_http_client_open(client, dataToWrite);

  if (err == ESP_OK) {
    esp_http_client_write(client, headerContent, strlen(headerContent));
    esp_http_client_write(client, (const char *)photoDataBuffer, photoDataSize);
    esp_http_client_write(client, closure, strlen(closure));

    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    SUPLA_LOG_INFO("HTTP POST Status = %d", status);
    if (true) {  // if (content_length > 0) {
      if (resultBuffer) {
        memset(resultBuffer, 0, resultBufferSize);
        while (true) {
          int dataRead = esp_http_client_read(
              client, resultBuffer, resultBufferSize - 1);
          if (dataRead < 0) {
            SUPLA_LOG_ERROR("data read error");
            break;
          } else if (dataRead > 0) {
            resultBuffer[dataRead < resultBufferSize ? dataRead
              : resultBufferSize - 1] =
              '\0';
          } else if (dataRead == 0) {
            if (errno == ECONNRESET || errno == ENOTCONN) {
              SUPLA_LOG_DEBUG("Connection closed, errno = %d", errno);
              break;
            }
            if (esp_http_client_is_complete_data_received(client) == true) {
              break;
            }
          }
        }

        if (!esp_http_client_is_complete_data_received(client)) {
          SUPLA_LOG_ERROR("error in receiving check update response");
        } else {
          SUPLA_LOG_INFO("HTTP POST complete, content = %s", resultBuffer);
          if (status >= 200 && status <= 202) {
            success = true;
          }
        }
      }
    }
  } else {
    SUPLA_LOG_ERROR("HTTP POST request failed: %s", esp_err_to_name(err));
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  return success;
}

namespace {
static char *responseBuffer = nullptr;
static size_t maxResponseLen = 0;
static size_t responseBufferOffset = 0;

static esp_err_t _http_event_handler(esp_http_client_event_t *evt) {
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      break;
    case HTTP_EVENT_ON_CONNECTED:
      break;
    case HTTP_EVENT_HEADER_SENT:
      break;
    case HTTP_EVENT_ON_HEADER:
      break;
    case HTTP_EVENT_ON_DATA:
      if (!esp_http_client_is_chunked_response(evt->client)) {
        if (responseBuffer == NULL) {
          SUPLA_LOG_ERROR("HTTP: response buffer is not set");
          return ESP_FAIL;
        }
        if (evt->data_len + responseBufferOffset > maxResponseLen) {
          SUPLA_LOG_ERROR("HTTP: response buffer is too small");
          return ESP_FAIL;
        }
        memcpy(responseBuffer + responseBufferOffset, evt->data, evt->data_len);
        responseBufferOffset += evt->data_len;
      } else {
        SUPLA_LOG_ERROR("Chunked response not supported");
      }
      break;
    case HTTP_EVENT_ON_FINISH:
      break;
    case HTTP_EVENT_DISCONNECTED:
      break;
    case HTTP_EVENT_REDIRECT:
      break;
  }
  return ESP_OK;
}
}  // namespace

bool OcrIc::getStatusFromOcrServer(const char *url,
                                   const char *authkey,
                                   char *resultBuffer,
                                   int resultBufferSize) {
  if (url == nullptr || url[0] == '\0' || authkey == nullptr ||
      authkey[0] == '\0' || resultBuffer == nullptr || resultBufferSize == 0) {
    return false;
  }

  if (responseBuffer != nullptr || maxResponseLen > 0) {
    SUPLA_LOG_ERROR("HTTP: response buffer is already set");
    return false;
  }
  bool result = false;

  memset(resultBuffer, 0, resultBufferSize);
  esp_http_client_config_t config = {};
  config.url = url;
  config.event_handler = nullptr;
  config.event_handler = _http_event_handler;

  responseBuffer = resultBuffer;
  maxResponseLen = resultBufferSize;
  responseBufferOffset = 0;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_method(client, HTTP_METHOD_GET);
  esp_http_client_set_header(client, "X-AuthKey", authkey);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK) {
    esp_http_client_fetch_headers(client);
    int status = esp_http_client_get_status_code(client);
    int contentLength = esp_http_client_get_content_length(client);
    (void)(contentLength);
    SUPLA_LOG_INFO(
        "HTTP GET Status = %d, content_length = %d", status, contentLength);
    SUPLA_LOG_INFO("HTTP GET complete, content = %s", resultBuffer);
    if (status >= 200 && status <= 202) {
      result = true;
    }
  } else {
    SUPLA_LOG_ERROR("HTTP GET request failed: %s", esp_err_to_name(err));
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  responseBuffer = nullptr;
  maxResponseLen = 0;
  responseBufferOffset = 0;

  return result;
}

void OcrIc::setLedState(int state) {
  if (ledGpio >= 0) {
    int duty = (state / 100.0) * 8192;
    if (duty > 8192) {
      duty = 8192;
    }
    if (duty < 0) {
      duty = 0;
    }

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL_1);
  }
}
