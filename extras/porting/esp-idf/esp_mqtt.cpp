// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "esp_mqtt.h"

#include <errno.h>

#include <SuplaDevice.h>
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) && \
    CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include <esp_crt_bundle.h>
#endif
#include <esp_tls_errors.h>
#include <mbedtls/x509.h>
#include <supla/auto_lock.h>
#include <supla/log_wrapper.h>
#include <supla/mutex.h>
#include <supla/storage/config.h>
#include <supla/time.h>

Supla::Mutex *Supla::Protocol::EspMqtt::mutex = nullptr;
Supla::Mutex *Supla::Protocol::EspMqtt::mutexEventHandler = nullptr;
static Supla::Protocol::EspMqtt *espMqtt = nullptr;

namespace Supla {
enum EspMqttStatus {
  EspMqttStatus_none,
  EspMqttStatus_transportError,
  EspMqttStatus_badProtocol,
  EspMqttStatus_serverUnavailable,
  EspMqttStatus_badUsernameOrPassword,
  EspMqttStatus_notAuthorized,
  EspMqttStatus_connectionRefused,
  EspMqttStatus_unknownConnectionError,
  EspMqttStatus_connected
};
}  // namespace Supla

static Supla::EspMqttStatus lastError = Supla::EspMqttStatus_none;

namespace {
Supla::ConnectionError mapMqttTransportError(
    const esp_mqtt_error_codes_t *error) {
  if (error == nullptr) {
    return Supla::ConnectionError::UNKNOWN;
  }
  if (error->esp_tls_cert_verify_flags & MBEDTLS_X509_BADCERT_EXPIRED) {
    return Supla::ConnectionError::CERTIFICATE_EXPIRED;
  }
  if (error->esp_tls_cert_verify_flags & MBEDTLS_X509_BADCERT_FUTURE) {
    return Supla::ConnectionError::CERTIFICATE_NOT_YET_VALID;
  }
  if (error->esp_tls_cert_verify_flags & MBEDTLS_X509_BADCERT_CN_MISMATCH) {
    return Supla::ConnectionError::HOSTNAME_MISMATCH;
  }
  if (error->esp_tls_cert_verify_flags & MBEDTLS_X509_BADCERT_NOT_TRUSTED) {
    return Supla::ConnectionError::UNTRUSTED_CERTIFICATE;
  }
  if (error->esp_tls_cert_verify_flags != 0 ||
      error->esp_tls_stack_err == -MBEDTLS_ERR_X509_CERT_VERIFY_FAILED ||
      error->esp_tls_last_esp_err == ESP_ERR_MBEDTLS_X509_CRT_PARSE_FAILED) {
    return Supla::ConnectionError::CERTIFICATE_ERROR;
  }

  switch (error->esp_tls_last_esp_err) {
    case ESP_ERR_ESP_TLS_CANNOT_RESOLVE_HOSTNAME:
      return Supla::ConnectionError::DNS;
    case ESP_ERR_ESP_TLS_CONNECTION_TIMEOUT:
      return Supla::ConnectionError::CONNECTION_TIMEOUT;
    case ESP_ERR_ESP_TLS_FAILED_CONNECT_TO_HOST:
      return Supla::ConnectionError::CONNECTION_REFUSED;
    case ESP_ERR_ESP_TLS_TCP_CLOSED_FIN:
      return Supla::ConnectionError::CONNECTION_LOST;
    case ESP_ERR_MBEDTLS_SSL_HANDSHAKE_FAILED:
      return Supla::ConnectionError::TLS_ERROR;
    default:
      break;
  }
  if (error->esp_transport_sock_errno == ETIMEDOUT) {
    return Supla::ConnectionError::CONNECTION_TIMEOUT;
  }
  if (error->esp_transport_sock_errno == ECONNREFUSED) {
    return Supla::ConnectionError::CONNECTION_REFUSED;
  }
  if (error->esp_transport_sock_errno == ECONNRESET ||
      error->esp_transport_sock_errno == ECONNABORTED ||
      error->esp_transport_sock_errno == ENOTCONN ||
      error->esp_transport_sock_errno == EPIPE) {
    return Supla::ConnectionError::CONNECTION_LOST;
  }
  return Supla::ConnectionError::UNKNOWN;
}
}  // namespace

void mqttEventDataProcess(esp_mqtt_event_handle_t event) {
  if (event == nullptr) {
    return;
  }

  char topic[MAX_TOPIC_LEN] = {};
  char payload[MQTT_MAX_PAYLOAD_LEN] = {};
  int topicLen = MAX_TOPIC_LEN;
  if (event->topic_len < topicLen) {
    topicLen = event->topic_len;
  }
  int payloadLen = MQTT_MAX_PAYLOAD_LEN - 1;
  if (event->data_len < payloadLen) {
    payloadLen = event->data_len;
  }

  strncpy(topic, event->topic, topicLen);
  strncpy(payload, event->data, payloadLen);

  espMqtt->processData(topic, payload);
}

static void mqttEventHandler(void *handler_args,
                             esp_event_base_t base,
                             int32_t eventId,
                             void *eventData) {
  SUPLA_LOG_DEBUG(" *** MQTT event handler enter");
  Supla::AutoLock eventHandlerLock(Supla::Protocol::EspMqtt::mutexEventHandler);
  SUPLA_LOG_DEBUG(" *** MQTT event handler waiting");
  Supla::AutoLock autoLock(Supla::Protocol::EspMqtt::mutex);
  SUPLA_LOG_DEBUG(
      " *** MQTT Event dispatched from event loop base=%s, eventId=%d",
      base,
      eventId);
  esp_mqtt_event_handle_t event =
      reinterpret_cast<esp_mqtt_event_handle_t>(eventData);
  switch (static_cast<esp_mqtt_event_id_t>(eventId)) {
    case MQTT_EVENT_BEFORE_CONNECT:
      SUPLA_LOG_DEBUG("MQTT_EVENT_BEFORE_CONNECT");
      espMqtt->setConnecting();
      break;
    case MQTT_EVENT_CONNECTED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_CONNECTED");
      if (lastError != Supla::EspMqttStatus_connected) {
        lastError = Supla::EspMqttStatus_connected;
        espMqtt->getSdc()->addLastStateLog("MQTT: connected");
      }
      espMqtt->setRegisteredAndReady();
      break;
    case MQTT_EVENT_DISCONNECTED:
      SUPLA_LOG_DEBUG("MQTT_EVENT_DISCONNECTED");
      if (lastError == Supla::EspMqttStatus_connected) {
        espMqtt->setConnectionError(Supla::ConnectionError::CONNECTION_LOST);
      } else {
        espMqtt->setConnecting();
      }
      break;

    case MQTT_EVENT_DATA:
      SUPLA_LOG_DEBUG("MQTT_EVENT_DATA");
      mqttEventDataProcess(event);
      break;

    case MQTT_EVENT_ERROR:
      SUPLA_LOG_DEBUG("MQTT_EVENT_ERROR");
      // MQTT_ERROR_TYPE_ESP_TLS  for backward compatibility
      // On ESP32 it was replaced with MQTT_ERROR_TYPE_TCP_TRANSPORT
      if (event->error_handle->error_type == MQTT_ERROR_TYPE_ESP_TLS) {
        espMqtt->setConnectionError(mapMqttTransportError(event->error_handle));
        if (lastError != Supla::EspMqttStatus_transportError) {
          lastError = Supla::EspMqttStatus_transportError;
          espMqtt->getSdc()->addLastStateLog(
              "MQTT: failed to establish connection");
        }
        SUPLA_LOG_DEBUG("Last error code reported from esp-tls: 0x%x",
                        event->error_handle->esp_tls_last_esp_err);
        SUPLA_LOG_DEBUG("Last tls stack error number: 0x%x",
                        event->error_handle->esp_tls_stack_err);
        SUPLA_LOG_DEBUG(
            "Last captured errno : %d (%s)",
            event->error_handle->esp_transport_sock_errno,
            strerror(event->error_handle->esp_transport_sock_errno));
      } else if (event->error_handle->error_type ==
                 MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        switch (event->error_handle->connect_return_code) {
          case MQTT_CONNECTION_REFUSE_PROTOCOL:
            espMqtt->setConnectionError(
                Supla::ConnectionError::PROTOCOL_ERROR);
            if (lastError != Supla::EspMqttStatus_badProtocol) {
              lastError = Supla::EspMqttStatus_badProtocol;
              espMqtt->getSdc()->addLastStateLog(
                  "MQTT: connection refused (bad protocol)");
            }
            break;
          case MQTT_CONNECTION_REFUSE_SERVER_UNAVAILABLE:
            espMqtt->setConnectionError(
                Supla::ConnectionError::SERVER_UNAVAILABLE);
            if (lastError != Supla::EspMqttStatus_serverUnavailable) {
              lastError = Supla::EspMqttStatus_serverUnavailable;
              espMqtt->getSdc()->addLastStateLog(
                  "MQTT: connection refused (server unavailable)");
            }
            break;
          case MQTT_CONNECTION_REFUSE_BAD_USERNAME:
            espMqtt->setConnectionError(
                Supla::ConnectionError::BAD_CREDENTIALS);
            if (lastError != Supla::EspMqttStatus_badUsernameOrPassword) {
              lastError = Supla::EspMqttStatus_badUsernameOrPassword;
              espMqtt->getSdc()->addLastStateLog(
                  "MQTT: connection refused (bad username or password)");
            }
            break;
          case MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED:
            espMqtt->setConnectionError(
                Supla::ConnectionError::NOT_AUTHORIZED);
            if (lastError != Supla::EspMqttStatus_notAuthorized) {
              lastError = Supla::EspMqttStatus_notAuthorized;
              espMqtt->getSdc()->addLastStateLog(
                  "MQTT: connection refused (not authorized)");
            }
            break;
          default:
            espMqtt->setConnectionError(
                Supla::ConnectionError::CONNECTION_REFUSED);
            if (lastError != Supla::EspMqttStatus_connectionRefused) {
              lastError = Supla::EspMqttStatus_connectionRefused;
              espMqtt->getSdc()->addLastStateLog("MQTT: connection refused");
            }
            break;
        }
        SUPLA_LOG_DEBUG("Connection refused error: 0x%x",
                        event->error_handle->connect_return_code);
      } else {
        espMqtt->setConnectionError(Supla::ConnectionError::UNKNOWN);
        if (lastError != Supla::EspMqttStatus_unknownConnectionError) {
          lastError = Supla::EspMqttStatus_unknownConnectionError;
          espMqtt->getSdc()->addLastStateLog("MQTT: other connection error");
        }
        SUPLA_LOG_DEBUG("Unknown error type: 0x%x",
                        event->error_handle->error_type);
      }
      break;
    default:
      break;
  }
}

Supla::Protocol::EspMqtt::EspMqtt(SuplaDeviceClass *sdc)
    : Supla::Protocol::Mqtt(sdc) {
  espMqtt = this;
}

Supla::Protocol::EspMqtt::~EspMqtt() {
  if (client != nullptr) {
    if (started) {
      esp_mqtt_client_stop(client);
    }
    esp_mqtt_client_destroy(client);
    client = nullptr;
  }
  delete[] mqttCaCert;
  mqttCaCert = nullptr;
  espMqtt = nullptr;
}

void Supla::Protocol::EspMqtt::onInit() {
  if (mutex == nullptr) {
    mutex = Supla::Mutex::Create();
    mutex->lock();
    mutexEventHandler = Supla::Mutex::Create();
  }
  if (!isEnabled()) {
    return;
  }
  Supla::Protocol::Mqtt::onInit();

  esp_mqtt_client_config_t mqttCfg = {};

  char clientId[MQTT_CLIENTID_MAX_SIZE] = {};
  generateClientId(clientId);

  MqttTopic lastWill(prefix);
  lastWill = lastWill / "state" / "connected";

  mqttCfg.broker.address.hostname = server;
  mqttCfg.broker.address.port = port;
  if (useAuth) {
    mqttCfg.credentials.username = user;
    mqttCfg.credentials.authentication.password = password;
  }
  if (useTls) {
    mqttCfg.broker.address.transport = MQTT_TRANSPORT_OVER_SSL;

    delete[] mqttCaCert;
    mqttCaCert = nullptr;
    auto cfg = Supla::Storage::ConfigInstance();
    bool verifyBroker =
        cfg == nullptr || cfg->isMqttBrokerVerificationEnabled();
    if (verifyBroker) {
      int mqttCaSize = cfg == nullptr ? 0 : cfg->getMqttCASize();
      if (mqttCaSize > 0) {
        mqttCaCert = new char[mqttCaSize + 1]();
        if (mqttCaCert == nullptr ||
            !cfg->getMqttCA(mqttCaCert, mqttCaSize + 1) ||
            mqttCaCert[0] == '\0') {
          delete[] mqttCaCert;
          mqttCaCert = nullptr;
          SUPLA_LOG_ERROR("MQTT: failed to load broker CA certificate");
          enabled = false;
          error = true;
          connectionError = ConnectionError::CERTIFICATE_ERROR;
          return;
        }
        mqttCfg.broker.verification.certificate = mqttCaCert;
        mqttCfg.broker.verification.certificate_len = 0;
#if defined(CONFIG_MBEDTLS_CERTIFICATE_BUNDLE) && \
    CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
      } else {
        mqttCfg.broker.verification.crt_bundle_attach = esp_crt_bundle_attach;
#else
      } else {
        SUPLA_LOG_ERROR(
            "MQTT: TLS requires mqtt_ca or ESP certificate bundle");
        enabled = false;
        error = true;
        connectionError = ConnectionError::CERTIFICATE_ERROR;
        return;
#endif
      }
    } else {
      SUPLA_LOG_WARNING("MQTT: broker certificate verification is disabled");
    }
  } else {
    mqttCfg.broker.address.transport = MQTT_TRANSPORT_OVER_TCP;
  }
  mqttCfg.session.keepalive = sdc->getActivityTimeout();

  mqttCfg.session.last_will.topic = lastWill.c_str();
  mqttCfg.session.last_will.msg = "false";
  mqttCfg.session.last_will.retain = 1;

  mqttCfg.credentials.client_id = clientId;

  client = esp_mqtt_client_init(&mqttCfg);
  esp_mqtt_client_register_event(
      client, MQTT_EVENT_ANY, mqttEventHandler, nullptr);
  /*MQTT_EVENT_BEFORE_CONNECT
    MQTT_EVENT_CONNECTED
    MQTT_EVENT_DISCONNECTED
    MQTT_EVENT_DATA
    MQTT_EVENT_ERROR
    */
}

void Supla::Protocol::EspMqtt::disconnect() {
  if (!isEnabled()) {
    return;
  }

  if (started) {
    // Sometimes when Wi-Fi reconnects, mqtt client is not
    // shutting down correctly (stop or disconnect doesn't have any effect)
    // and in result mqtt_start results in "Client has started" message (with
    // ESP_FAIL return code). As a workaround we completly reinitialize
    // mqtt client instead of stopping and starting...
    if (lastError == Supla::EspMqttStatus_connected) {
      mutex->unlock();
      SUPLA_LOG_DEBUG("MQTT layer stop requested");
      esp_mqtt_client_stop(client);
      started = false;
    } else {
      esp_mqtt_client_disconnect(client);
      mutex->unlock();
      SUPLA_LOG_DEBUG("MQTT layer disconnect requested");
      esp_mqtt_client_stop(client);
      started = false;
    }
    esp_mqtt_client_destroy(client);
    client = nullptr;
    onInit();
    mutex->lock();
  }
  enterRegisteredAndReady = false;
}

void Supla::Protocol::EspMqtt::publishChannelSetup(int channelNumber) {
  if (channelNumber < 0 || channelNumber >= channelsCount) {
    return;
  }
  publishHADiscovery(channelNumber);
  subscribeChannel(channelNumber);
  publishChannelState(channelNumber);
  configChangedBit[channelNumber / 8] &= ~(1 << (channelNumber % 8));
}

bool Supla::Protocol::EspMqtt::iterate(uint32_t _millis) {
  if (!isEnabled()) {
    return false;
  }

  uptime.iterate(_millis);

  if (started) {
    // this is used to synchronize event loop handling with SuplaDevice.iterate
    mutex->unlock();
    mutexEventHandler->lock();
    mutex->lock();
    mutexEventHandler->unlock();

    // below code is executed after mqtt event loop from ...
    if (!connected) {
      return false;
    }

    if (enterRegisteredAndReady) {
      enterRegisteredAndReady = false;
      publishDeviceStatus(true);
      lastStatusUpdateSec = uptime.getConnectionUptime();

      for (int i = 0; i < channelsCount; i++) {
        publishChannelSetup(i);
      }
    }

    // check if any configChangedBit is set
    bool anyConfigChanged = false;
    for (int i = 0; i < sizeof(configChangedBit) / sizeof(configChangedBit[0]);
         i++) {
      if (configChangedBit[i] != 0) {
        anyConfigChanged = true;
        break;
      }
    }
    if (anyConfigChanged) {
      for (int i = 0; i < channelsCount; i++) {
        if (configChangedBit[i / 8] & (1 << (i % 8))) {
          publishChannelSetup(i);
        }
      }
    }

    // send device status update
    if (uptime.getConnectionUptime() - lastStatusUpdateSec >= 5) {
      lastStatusUpdateSec = uptime.getConnectionUptime();
      publishDeviceStatus(false);
    }

    // send channel state updates are done in iterateConnected
    return true;
  } else {
    started = true;
    enterRegisteredAndReady = false;
    esp_mqtt_client_start(client);
  }
  return false;
}

void Supla::Protocol::EspMqtt::setConnecting() {
  connecting = true;
  connected = false;
  enterRegisteredAndReady = false;
}

void Supla::Protocol::EspMqtt::setConnectionError(ConnectionError newError) {
  error = true;
  connected = false;
  enterRegisteredAndReady = false;
  connectionError = newError;
}

void Supla::Protocol::EspMqtt::setRegisteredAndReady() {
  connecting = false;
  connected = true;
  error = false;
  connectionError = ConnectionError::NONE;
  enterRegisteredAndReady = true;
  uptime.resetConnectionUptime();
  lastStatusUpdateSec = 0;
  memset(configChangedBit, 0, sizeof(configChangedBit));
}

Supla::ConnectionError
Supla::Protocol::EspMqtt::getConnectionError() const {
  return connectionError;
}

void Supla::Protocol::EspMqtt::publishImp(const char *topic,
                                          const char *payload,
                                          int qos,
                                          bool retain) {
  if (!connected) {
    return;
  }

  mutex->unlock();
  esp_mqtt_client_publish(client, topic, payload, 0, qos, retain ? 1 : 0);
  mutex->lock();
}

void Supla::Protocol::EspMqtt::subscribeImp(const char *topic, int qos) {
  if (!connected) {
    return;
  }

  mutex->unlock();
  esp_mqtt_client_subscribe(client, topic, qos);
  mutex->lock();
}
