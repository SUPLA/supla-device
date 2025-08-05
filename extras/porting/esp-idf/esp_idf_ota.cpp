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

#include <cJSON.h>
#include <ctype.h>
#include <esp_http_client.h>
#include <esp_idf_ota.h>
#include <esp_ota_ops.h>
#include <esp_task_wdt.h>
#include <stdio.h>
#include <supla-common/log.h>
#include <supla/log_wrapper.h>
#include <supla/rsa_verificator.h>
#include <supla/sha256.h>
#include <supla/time.h>
#include <supla/tools.h>
#include <supla/device/register_device.h>

#include <cerrno>

#include "supla/device/sw_update.h"

#define BUFFER_SIZE 4096

#ifndef SUPLA_DEVICE_ESP32
// ESP8266 RTOS doesn't have OTA_WITH_SEQUENTIAL_WRITES, so we replace it with
// default OTA_SIZE_UNKNOWN for ESP8266 target.
#define OTA_WITH_SEQUENTIAL_WRITES OTA_SIZE_UNKNOWN
#endif

Supla::Device::SwUpdate *Supla::Device::SwUpdate::Create(SuplaDeviceClass *sdc,
                                                         const char *newUrl) {
  return new Supla::EspIdfOta(sdc, newUrl);
}

Supla::EspIdfOta::EspIdfOta(SuplaDeviceClass *sdc, const char *newUrl)
    : Supla::Device::SwUpdate(sdc, newUrl) {
}

Supla::EspIdfOta::~EspIdfOta() {
  if (client) {
    esp_http_client_cleanup(client);
    client = 0;
  }
  if (otaBuffer) {
    delete[] otaBuffer;
    otaBuffer = nullptr;
  }
  if (httpAgent) {
    free(httpAgent);
    httpAgent = nullptr;
  }
}

void Supla::EspIdfOta::iterate() {
  if (!isStarted()) {
    return;
  }

  if (httpAgent == nullptr) {
#define HTTP_AGENT_SIZE 100
    httpAgent = new char[HTTP_AGENT_SIZE];
    if (httpAgent) {
      Supla::RegisterDevice::generateHttpAgent(httpAgent, HTTP_AGENT_SIZE);
    }
  }
  finished = true;

#define URL_SIZE 512
#define BUF_SIZE 512

  char queryParams[URL_SIZE] = {};
  char buf[BUF_SIZE] = {};
  int curPos = 0;

  int v = 0;

  while (1) {
    v = stringAppend(queryParams + curPos,
                     "manufacturerId=",
                     URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;
    snprintf(
        buf, sizeof(buf), "%d", Supla::RegisterDevice::getManufacturerId());
    v = stringAppend(queryParams + curPos, buf, URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;

    v = stringAppend(
        queryParams + curPos, "&productId=", URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;
    snprintf(buf, sizeof(buf), "%d", Supla::RegisterDevice::getProductId());
    v = stringAppend(queryParams + curPos, buf, URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;

    v = stringAppend(
        queryParams + curPos, "&productName=", URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;
    urlEncode(Supla::RegisterDevice::getName(), buf, BUF_SIZE);
    v = stringAppend(queryParams + curPos, buf, URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;

    v = stringAppend(
        queryParams + curPos, "&platform=", URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;
    snprintf(buf, sizeof(buf), "%d", Supla::getPlatformId());
    v = stringAppend(queryParams + curPos, buf, URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;

    v = stringAppend(
        queryParams + curPos, "&version=", URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;
    urlEncode(Supla::RegisterDevice::getSoftVer(), buf, BUF_SIZE);
    v = stringAppend(queryParams + curPos, buf, URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;

    v = stringAppend(
        queryParams + curPos, "&guidHash=", URL_SIZE - curPos - 1);
    if (v == 0) break;
    curPos += v;
    {
      Supla::Sha256 hash;
      hash.update(
          reinterpret_cast<const uint8_t *>(Supla::RegisterDevice::getGUID()),
          SUPLA_GUID_SIZE);

      uint8_t sha[32] = {};
      hash.digest(sha);

      if (curPos < URL_SIZE - 1 - 32 * 2) {
        curPos += generateHexString(sha, queryParams + curPos, 32);
      } else {
        v = 0;
        break;
      }
    }

    if (!Supla::RegisterDevice::isEmailEmpty()) {
      v = stringAppend(
          queryParams + curPos, "&userEmailHash=", URL_SIZE - curPos - 1);
      if (v == 0) break;
      curPos += v;
      Supla::Sha256 hash;
      strncpy(buf, Supla::RegisterDevice::getEmail(), BUF_SIZE - 1);
      buf[BUF_SIZE - 1] = '\0';
      auto bufPtr = buf;
      int size = 0;
      while (*bufPtr) {
        *bufPtr = tolower(*bufPtr);
        bufPtr++;
        size++;
      }
      hash.update(reinterpret_cast<uint8_t *>(buf), size);
      uint8_t sha[32] = {};
      hash.digest(sha);
      if (curPos < URL_SIZE - 1 - 32 * 2) {
        curPos += generateHexString(sha, queryParams + curPos, 32);
      } else {
        v = 0;
        break;
      }
    }

    if (beta) {
      v = stringAppend(
          queryParams + curPos, "&beta=true", URL_SIZE - curPos - 1);
      if (v == 0) break;
      curPos += v;
    }

    if (isSecurityOnly()) {
      v = stringAppend(
          queryParams + curPos, "&securityOnly=true", URL_SIZE - curPos - 1);
      if (v == 0) break;
      curPos += v;
    }

    break;
  }

  if (v == 0) {
    fail("SW update: fail - too long request url");
    return;
  }
  SUPLA_LOG_INFO(
      "SW update: checking updates from url: \"%s\", with query: \"%s\"",
      url, queryParams);

  int querySize = strlen(queryParams);

  esp_http_client_config_t configCheckUpdate = {};
  configCheckUpdate.url = url;
  configCheckUpdate.timeout_ms = 5000;
  configCheckUpdate.user_agent = httpAgent;
  if (!skipCert && sdc && sdc->getSuplaCACert()) {
    SUPLA_LOG_INFO("SW update: using Supla CA cert");
    configCheckUpdate.cert_pem = sdc->getSuplaCACert();
  }

  if (client) {
    esp_http_client_cleanup(client);
    client = nullptr;
  }
  client = esp_http_client_init(&configCheckUpdate);
  if (client == nullptr) {
    fail("SW update: failed initialize connection with update server");
    return;
  }

  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_header(client,
                             "Content-Type",
                             "application/x-www-form-urlencoded");
  esp_err_t err;
  err = esp_http_client_open(client, querySize);
  if (err != ESP_OK) {
    fail("SW update: failed to open connection with update server");
    return;
  }

  esp_http_client_write(client, queryParams, querySize);
  esp_http_client_fetch_headers(client);

  SUPLA_LOG_DEBUG("Starting OTA");

  if (otaBuffer) {
    delete[] otaBuffer;
    otaBuffer = nullptr;
  }
  otaBuffer = new uint8_t[BUFFER_SIZE + 1];
  if (otaBuffer == nullptr) {
    fail("SW udpate: failed to allocate memory");
    return;
  }

  while (true) {
    int dataRead = esp_http_client_read(
        client, reinterpret_cast<char *>(otaBuffer), BUFFER_SIZE);
    if (dataRead < 0) {
      fail("SW update: data read error");
      return;
    } else if (dataRead > 0) {
      otaBuffer[dataRead] = '\0';
      SUPLA_LOG_DEBUG("Read: %s", otaBuffer);
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

  if (esp_http_client_is_complete_data_received(client) != true) {
    fail("SW update: error in receiving check update response");
    return;
  }
  cJSON *json = cJSON_Parse(reinterpret_cast<const char *>(otaBuffer));

  if (json == nullptr) {
    fail("SW update: failed to parse check update response");
    return;
  }
  cJSON *status = cJSON_GetObjectItemCaseSensitive(json, "status");
  cJSON *latestUpdate = cJSON_GetObjectItemCaseSensitive(json, "latestUpdate");
  if (cJSON_IsString(status) && (status->valuestring != NULL)) {
    snprintf(buf, BUF_SIZE, "SW update status: %s", status->valuestring);
    SUPLA_LOG_INFO("%s", buf);
    log(buf);
  }

  esp_http_client_cleanup(client);
  client = nullptr;

  if (cJSON_IsObject(latestUpdate)) {
    cJSON *version = cJSON_GetObjectItemCaseSensitive(latestUpdate, "version");
    cJSON *url = cJSON_GetObjectItemCaseSensitive(latestUpdate, "updateUrl");
    if (cJSON_IsString(version) && (version->valuestring != NULL) &&
        cJSON_IsString(url) && (url->valuestring != NULL)) {
      snprintf(
          buf, BUF_SIZE, "SW update new version: %s", version->valuestring);
      SUPLA_LOG_INFO("%s", buf);
      log(buf);
      snprintf(buf, BUF_SIZE, "SW update url: \"%s\"", url->valuestring);
      SUPLA_LOG_INFO("%s", buf);
      log(buf);

      // copy to newVersion
      if (newVersion) {
        delete[] newVersion;
      }
      int versionLen = strlen(version->valuestring) + 1;
      newVersion = new char[versionLen];
      if (newVersion) {
        snprintf(newVersion, versionLen, "%s", version->valuestring);
      }

      // copy to updateUrl
      if (updateUrl) {
        delete[] updateUrl;
      }
      int urlLen = strlen(url->valuestring) + 1;
      updateUrl = new char[urlLen];
      if (updateUrl == nullptr) {
        fail("SW update: failed to allocate memory");
        cJSON_Delete(json);
        return;
      }
      snprintf(updateUrl, urlLen, "%s", url->valuestring);

      // copy changelogUrl parameter (if available)
      cJSON *changelogUrlJson =
          cJSON_GetObjectItemCaseSensitive(latestUpdate, "changelogUrl");
      if (cJSON_IsString(changelogUrlJson) &&
          (changelogUrlJson->valuestring != NULL)) {
        if (changelogUrl) {
          delete[] changelogUrl;
        }

        int urlLen = strlen(changelogUrlJson->valuestring) + 1;
        if (urlLen < SUPLA_URL_PATH_MAXSIZE) {
          changelogUrl = new char[urlLen];
          if (changelogUrl) {
            snprintf(changelogUrl, urlLen, "%s", changelogUrlJson->valuestring);
          }
        } else {
          SUPLA_LOG_WARNING("SW update: changelogUrl too long, skipping");
        }
      } else {
        SUPLA_LOG_WARNING("SW update: no changelogUrl received");
      }
    } else {
      fail("SW update: no url and version received - finishing");
      cJSON_Delete(json);
      return;
    }
  } else {
    fail("SW update: no new update available");
    cJSON_Delete(json);
    return;
  }

  cJSON_Delete(json);

  if (checkUpdateAndAbort) {
    abort = true;
    return;
  }

  //////////////////
  // download update
  //////////////////
  esp_http_client_config_t configGet = {};
  configGet.url = updateUrl;
  configGet.timeout_ms = 10000;
  configGet.user_agent = httpAgent;
  if (!skipCert && sdc && sdc->getSuplaCACert()) {
    SUPLA_LOG_INFO("SW update: using Supla CA cert");
    configCheckUpdate.cert_pem = sdc->getSuplaCACert();
  }
  client = esp_http_client_init(&configGet);
  if (client == NULL) {
    fail("SW update: failed initialize GET connection with update server");
    return;
  }
  esp_http_client_set_method(client, HTTP_METHOD_GET);
  err = esp_http_client_open(client, 0);
  if (err != ESP_OK) {
    fail("SW update: failed to open HTTP connection");
    return;
  }
  err = esp_http_client_fetch_headers(client);
  if (err < 0) {
    fail("SW update: failed to read file from url");
    SUPLA_LOG_DEBUG("SW update: result %d", err);
    return;
  }

  int returnCode = esp_http_client_get_status_code(client);
  SUPLA_LOG_INFO("HTTP return code %d", returnCode);
  if (returnCode != 200) {
    snprintf(buf, BUF_SIZE, "SW update: HTTP GET failed with status code %d",
         returnCode);
    fail(buf);
    return;
  }

  // Start fetching bin file and perform update
  const esp_partition_t *updatePartition = NULL;

  updatePartition = esp_ota_get_next_update_partition(NULL);
  if (updatePartition == NULL) {
    fail("SW update: failed to get next update partition");
    return;
  }
  SUPLA_LOG_DEBUG("Used ota partition subtype %d, offset 0x%x",
                  updatePartition->subtype,
                  updatePartition->address);

  int binSize = 0;

  err =
    esp_ota_begin(updatePartition, OTA_WITH_SEQUENTIAL_WRITES, &updateHandle);
  if (err != ESP_OK) {
    fail("SW update: OTA begin failed");
    return;
  }

  SUPLA_LOG_DEBUG("Getting file from server...");
  int bytesRead = 0;
  int bytesReadPrinted = 0;
  while (true) {
    int dataRead = esp_http_client_read(
        client, reinterpret_cast<char *>(otaBuffer), BUFFER_SIZE);
    if (dataRead < 0) {
      fail("SW update: data read error");
      return;
    } else if (dataRead > 0) {
      bytesRead += dataRead;
      if (bytesRead - bytesReadPrinted > 1024 * 100) {
        bytesReadPrinted = bytesRead;
        SUPLA_LOG_DEBUG("SW update: downloaded %d bytes...", bytesRead);
      }
      err = esp_ota_write(updateHandle, (const void *)otaBuffer, dataRead);
      if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
        fail("SW update: image corrupted - invalid magic byte");
        return;
      }
      if (err != ESP_OK) {
        fail("SW update: flash write fail");
        return;
      }
      binSize += dataRead;
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
  SUPLA_LOG_INFO("Download complete. Wrote %d bytes", binSize);
  if (esp_http_client_is_complete_data_received(client) != true) {
    fail("SW update: error in receiving complete file");
    return;
  }

  esp_http_client_cleanup(client);
  client = nullptr;

  err = esp_ota_end(updateHandle);
  updateHandle = 0;

  if (err != ESP_OK) {
    if (err == ESP_ERR_OTA_VALIDATE_FAILED) {
      fail("SW update: image validation failed - image is corrupted");
    } else {
      fail("SW update: OTA end failed");
    }
    return;
  }

  // If we are here, then file was correctly downloaded and stored on flash.
  // We apply here additional RSA signature check added to Supla firmware

  if (!verifyRsaSignature(updatePartition, binSize)) {
    fail("SW update: RSA signature verification failed");
    return;
  }

  // RSA signature check done
  err = esp_ota_set_boot_partition(updatePartition);
  if (err != ESP_OK) {
    fail("SW update: failed to set boot partition");
    return;
  }
  delete[] otaBuffer;
  otaBuffer = nullptr;
  return;
}

#define RSA_FOOTER_SIZE 16
bool Supla::EspIdfOta::verifyRsaSignature(
    const esp_partition_t *updatePartition, int binSize) {
  uint8_t footer[RSA_FOOTER_SIZE] = {};
  // footer contain some information, however supla-esp-signtool is able to
  // produce only one footer, so here we hardcode it. Please check
  // supla-esp-signtool for details.
  const uint8_t expectedFooter[RSA_FOOTER_SIZE] = {0xBA,
                                                   0xBE,
                                                   0x2B,
                                                   0xED,
                                                   0x00,
                                                   0x01,
                                                   0x02,
                                                   0x00,
                                                   0x10,
                                                   0x00,
                                                   0x00,
                                                   0x00,
                                                   0x00,
                                                   0x00,
                                                   0x00,
                                                   0x00};

  Supla::Sha256 hash;

  if (binSize < RSA_FOOTER_SIZE + RSA_NUM_BYTES + 1) {
    SUPLA_LOG_ERROR("Fail: bin size too small");
    return false;
  }

  int appSize = binSize - RSA_FOOTER_SIZE - RSA_NUM_BYTES;

  for (int i = 0; i < appSize; i += BUFFER_SIZE) {
    int sizeToRead = BUFFER_SIZE;
    if (i + sizeToRead > appSize) {
      sizeToRead = appSize - i;
    }
    esp_err_t err =
        esp_partition_read(updatePartition, i, otaBuffer, sizeToRead);

    if (err != ESP_OK) {
      SUPLA_LOG_ERROR("Fail: error reading app");
      return false;
    }

    hash.update(otaBuffer, sizeToRead);
  }

  esp_err_t err = esp_partition_read(
      updatePartition, binSize - RSA_FOOTER_SIZE, footer, RSA_FOOTER_SIZE);

  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Fail: error reading footer");
    return false;
  }

  if (memcmp(footer, expectedFooter, RSA_FOOTER_SIZE) != 0) {
    SUPLA_LOG_ERROR("Fail: invalid footer");
    return false;
  }

  err = esp_partition_read(updatePartition,
                           binSize - RSA_FOOTER_SIZE - RSA_NUM_BYTES,
                           otaBuffer,
                           RSA_NUM_BYTES);

  if (err != ESP_OK) {
    SUPLA_LOG_ERROR("Fail: error reading signature");
    return false;
  }

  if (sdc && sdc->getRsaPublicKey()) {
    Supla::RsaVerificator rsa(sdc->getRsaPublicKey());
    if (rsa.verify(&hash, otaBuffer)) {
      SUPLA_LOG_DEBUG("RSA signature verification successful");
      return true;
    } else {
      SUPLA_LOG_ERROR("Fail: Incorrect RSA signature");
      return false;
    }
  } else {
    SUPLA_LOG_ERROR("Fail: RSA public key not set in SuplaDevice instance");
    return false;
  }
  return false;
}

void Supla::EspIdfOta::fail(const char *reason) {
  if (otaBuffer) {
    delete[] otaBuffer;
    otaBuffer = nullptr;
  }

  if (client) {
    esp_http_client_cleanup(client);
    client = 0;
  }
  if (updateHandle) {
    esp_ota_end(updateHandle);
    updateHandle = 0;
  }

  log(reason);
  abort = true;
}

void Supla::EspIdfOta::log(const char *value) {
  if (sdc && !checkUpdateAndAbort) {
    sdc->addLastStateLog(value);
  }
}
