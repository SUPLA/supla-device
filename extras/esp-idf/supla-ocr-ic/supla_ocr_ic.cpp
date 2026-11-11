// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "supla_ocr_ic.h"

#include <string.h>
#include <sys/socket.h>

#include <driver/ledc.h>
#include <esp_http_client.h>
#include <esp_timer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <supla/io.h>
#include <supla/log_wrapper.h>

#include "ocr_http_response_buffer.h"

namespace {
class OcrResponseTimer {
 public:
  OcrResponseTimer(int socket, int64_t deadlineUs)
      : socket(socket), deadlineUs(deadlineUs) {
    started = start();
  }
  ~OcrResponseTimer() {
    // A fired one-shot is inactive even while its callback is still running.
    if (started && esp_timer_stop(timer) != ESP_OK) {
      xSemaphoreTake(done, portMAX_DELAY);
    }
    if (timer) {
      esp_timer_delete(timer);
    }
    if (done) {
      vSemaphoreDelete(done);
    }
  }
  bool isStarted() const { return started; }

  OcrResponseTimer(const OcrResponseTimer &) = delete;
  OcrResponseTimer &operator=(const OcrResponseTimer &) = delete;

 private:
  static void expire(void *context) {
    auto *response = static_cast<OcrResponseTimer *>(context);
    ::shutdown(response->socket, SHUT_RDWR);
    xSemaphoreGive(response->done);
  }
  bool start() {
    if (socket < 0) {
      return false;
    }
    done = xSemaphoreCreateBinary();
    if (!done) {
      return false;
    }
    esp_timer_create_args_t args = {};
    args.callback = expire;
    args.arg = this;
    args.name = "ocr_response";
    if (esp_timer_create(&args, &timer) != ESP_OK) {
      return false;
    }
    const int64_t remainingUs = deadlineUs - esp_timer_get_time();
    return remainingUs > 0 &&
        esp_timer_start_once(timer, remainingUs) == ESP_OK;
  }
  int socket;
  int64_t deadlineUs;
  esp_timer_handle_t timer = nullptr;
  SemaphoreHandle_t done = nullptr;
  bool started = false;
};
}  // namespace

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
    int resultBufferSize,
    const char *cropConfig) {
  // TODO(klew): move it to common implementation based on
  // Supla::Network::Client
  bool success = false;
  if (resultBuffer == nullptr || resultBufferSize <= 0) {
    return false;
  }
  resultBuffer[0] = '\0';

  const char *headerContent =
    "------WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
    "Content-Disposition: form-data; name=\"image\"; "
    "filename=\"image.jpg\"\r\n"
    "Content-Type: image/jpeg\r\n\r\n";
  const char *closure = "\r\n\r\n------WebKitFormBoundary7MA4YWxkTrZu0gW--\r\n";

  const char *headerCropContent =
    "------WebKitFormBoundary7MA4YWxkTrZu0gW\r\n"
    "Content-Disposition: form-data; name=\"cropConfig\";\r\n"
    "Content-Type: application/json\r\n\r\n";

  size_t body_size = strlen(headerContent);

  esp_http_client_config_t config = {};
  config.url = url;
  config.event_handler = nullptr;

  esp_http_client_handle_t client = esp_http_client_init(&config);
  if (client == nullptr) {
    return false;
  }

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(
      client, "X-AuthKey", authkey);
  esp_http_client_set_header(
      client,
      "Content-Type",
      "multipart/form-data; boundary=----WebKitFormBoundary7MA4YWxkTrZu0gW");

  int dataToWrite = body_size + photoDataSize + strlen(closure);
  if (testMode) {
    dataToWrite +=
        strlen(headerCropContent) + strlen(cropConfig) + strlen(closure);
  }

  esp_err_t err =
    esp_http_client_open(client, dataToWrite);

  if (err == ESP_OK) {
    esp_http_client_write(client, headerContent, strlen(headerContent));
    esp_http_client_write(client, (const char *)photoDataBuffer, photoDataSize);
    esp_http_client_write(client, closure, strlen(closure));

    if (testMode) {
      esp_http_client_write(
          client, headerCropContent, strlen(headerCropContent));
      esp_http_client_write(client, cropConfig, strlen(cropConfig));
      esp_http_client_write(client, closure, strlen(closure));
    }

    const int64_t responseStartUs = esp_timer_get_time();
    const int64_t responseDeadlineUs = responseStartUs + 10000000;
    OcrResponseTimer deadline(esp_http_client_get_socket(client),
                              responseDeadlineUs);
    if (deadline.isStarted()) {
      const int64_t contentLength = esp_http_client_fetch_headers(client);
      int status = esp_http_client_get_status_code(client);
      SUPLA_LOG_INFO("HTTP POST Status = %d", status);
      bool complete = false;
      size_t offset = 0;
      char data[128];
      if (contentLength >= 0 && contentLength < resultBufferSize) {
        while (true) {
          const int64_t remainingUs = responseDeadlineUs - esp_timer_get_time();
          if (remainingUs <= 0 ||
              esp_http_client_set_timeout_ms(
                  client, (remainingUs + 999) / 1000) != ESP_OK) {
            break;
          }
          // Read at most one byte beyond capacity to detect oversized bodies.
          const size_t available = resultBufferSize - offset;
          const int length =
              available < sizeof(data) ? available : sizeof(data);
          const int received = esp_http_client_read(client, data, length);
          if (received < 0 || received > length ||
              esp_timer_get_time() >= responseDeadlineUs) {
            break;
          }
          if (received == 0) {
            // Drain cached body data before accepting the completion flag.
            complete = esp_http_client_is_complete_data_received(client);
            break;
          }
          if (!Supla::Sensor::appendOcrHttpResponseData(
                  resultBuffer, resultBufferSize, &offset, data, received)) {
            break;
          }
        }
      }
      if (complete) {
        SUPLA_LOG_INFO("HTTP POST complete, content = %s", resultBuffer);
        success = status >= 200 && status <= 202;
      } else {
        SUPLA_LOG_ERROR("HTTP POST response incomplete or exceeds limits");
      }
    } else {
      SUPLA_LOG_ERROR("HTTP POST response deadline could not be started");
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
static bool responseBufferOverflow = false;

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
        if (evt->data_len < 0 ||
            !Supla::Sensor::appendOcrHttpResponseData(
                responseBuffer,
                maxResponseLen,
                &responseBufferOffset,
                static_cast<const char *>(evt->data),
                static_cast<size_t>(evt->data_len))) {
          SUPLA_LOG_ERROR("HTTP: response buffer is too small");
          responseBufferOverflow = true;
          return ESP_FAIL;
        }
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
      authkey[0] == '\0' || resultBuffer == nullptr || resultBufferSize <= 0) {
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
  responseBufferOverflow = false;

  esp_http_client_handle_t client = esp_http_client_init(&config);

  esp_http_client_set_method(client, HTTP_METHOD_GET);
  esp_http_client_set_header(client, "X-AuthKey", authkey);
  esp_err_t err = esp_http_client_perform(client);

  if (err == ESP_OK && !responseBufferOverflow) {
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
  } else if (responseBufferOverflow) {
    SUPLA_LOG_ERROR("HTTP GET response is too large");
  } else {
    SUPLA_LOG_ERROR("HTTP GET request failed: %s", esp_err_to_name(err));
  }

  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  responseBuffer = nullptr;
  maxResponseLen = 0;
  responseBufferOffset = 0;
  responseBufferOverflow = false;

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
