// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "mqtt_client.h"

#include <pthread.h>
#include <supla-common/tools.h>
#include <supla/network/client.h>
#include <supla/time.h>
#include <unistd.h>
#include <openssl/bio.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>

#include "linux_client.h"
#include "linux_mqtt_client.h"

pthread_t mqtt_deamon_thread = 0;
static std::atomic<bool> mqtt_loop_running(false);

struct reconnect_state_t* reconnect_state;

int delay_time = 5;
void reconnect_client(struct mqtt_client* client, void** reconnect_state_vptr);


struct SuplaMqttBio {
  Supla::Client* client = nullptr;
  std::string caCert;
};

int supla_mqtt_bio_create(BIO* bio) {
  BIO_set_init(bio, 1);
  BIO_set_data(bio, nullptr);
  return 1;
}

int supla_mqtt_bio_destroy(BIO* bio) {
  if (bio == nullptr) {
    return 0;
  }
  auto* transport = static_cast<SuplaMqttBio*>(BIO_get_data(bio));
  if (transport != nullptr) {
    delete transport->client;
    delete transport;
  }
  BIO_set_data(bio, nullptr);
  BIO_set_init(bio, 0);
  return 1;
}

int supla_mqtt_bio_read(BIO* bio, char* out, int outl) {
  if (out == nullptr || outl <= 0) {
    return 0;
  }
  auto* transport = static_cast<SuplaMqttBio*>(BIO_get_data(bio));
  if (transport == nullptr || transport->client == nullptr) {
    return 0;
  }

  BIO_clear_retry_flags(bio);
  int result = transport->client->read(reinterpret_cast<uint8_t*>(out), outl);
  if (result > 0) {
    return result;
  }
  if (result < 0 && transport->client->connected()) {
    BIO_set_retry_read(bio);
    return -1;
  }
  return 0;
}

int supla_mqtt_bio_write(BIO* bio, const char* in, int inl) {
  if (in == nullptr || inl <= 0) {
    return 0;
  }
  auto* transport = static_cast<SuplaMqttBio*>(BIO_get_data(bio));
  if (transport == nullptr || transport->client == nullptr) {
    return 0;
  }

  BIO_clear_retry_flags(bio);
  size_t result = transport->client->write(reinterpret_cast<const uint8_t*>(in),
                                           static_cast<size_t>(inl));
  if (result > 0) {
    return static_cast<int>(result);
  }
  if (transport->client->connected()) {
    BIO_set_retry_write(bio);
    return -1;
  }
  return 0;
}

long supla_mqtt_bio_ctrl(BIO* bio,  // NOLINT(runtime/int)
                         int cmd,
                         long num,  // NOLINT(runtime/int)
                         void* ptr) {
  (void)(bio);
  (void)(num);
  (void)(ptr);
  switch (cmd) {
    case BIO_CTRL_FLUSH:
      return 1;
    default:
      return 0;
  }
}

BIO_METHOD* supla_mqtt_bio_method() {
  static BIO_METHOD* method = nullptr;
  if (method == nullptr) {
    method = BIO_meth_new(BIO_TYPE_SOURCE_SINK, "supla mqtt client");
    BIO_meth_set_write(method, supla_mqtt_bio_write);
    BIO_meth_set_read(method, supla_mqtt_bio_read);
    BIO_meth_set_ctrl(method, supla_mqtt_bio_ctrl);
    BIO_meth_set_create(method, supla_mqtt_bio_create);
    BIO_meth_set_destroy(method, supla_mqtt_bio_destroy);
  }
  return method;
}

std::string readFile(const std::string& path) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("failed to open ca certificate");
  }
  std::ostringstream content;
  content << file.rdbuf();
  return content.str();
}

BIO* open_supla_client(const reconnect_state_t& state) {
  auto* transport = new SuplaMqttBio();
  transport->client = Supla::ClientBuilder();
  if (transport->client == nullptr) {
    delete transport;
    return nullptr;
  }

  transport->client->setSSLEnabled(state.useSSL);
  if (state.useSSL) {
    if (state.verifyCA) {
      auto* linuxClient = dynamic_cast<Supla::LinuxClient*>(transport->client);
      if (!state.fileCA.empty()) {
        transport->caCert = readFile(state.fileCA);
        transport->client->setCACert(transport->caCert.c_str());
      } else if (linuxClient != nullptr) {
        linuxClient->setUseDefaultCACerts(true);
      } else {
        SUPLA_LOG_ERROR("MQTT CA verification requires LinuxClient");
        delete transport->client;
        delete transport;
        return nullptr;
      }
    } else {
      SUPLA_LOG_WARNING(
          "Connecting to MQTT broker without certificate validation "
          "(INSECURE)");
    }
  }

  if (!transport->client->connect(state.hostname.c_str(), state.port)) {
    delete transport->client;
    delete transport;
    return nullptr;
  }

  BIO* bio = BIO_new(supla_mqtt_bio_method());
  if (bio == nullptr) {
    delete transport->client;
    delete transport;
    return nullptr;
  }
  BIO_set_data(bio, transport);
  BIO_set_init(bio, 1);
  return bio;
}

void* mqtt_client_loop(void* client) {
  (void)client;
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  SUPLA_LOG_DEBUG("Start MQTT client loop...");
  while (st_app_terminate == 0 && mqtt_loop_running) {
    if (mq_client != nullptr) {
      mqtt_sync((struct mqtt_client*)mq_client);
    }
    delay(100);
  }
  SUPLA_LOG_DEBUG("Stop MQTT client loop.");
  return nullptr;
}

int mqtt_client_init(std::string addr,
                     int port,
                     std::string username,
                     std::string password,
                     std::string client_name,
                     const std::unordered_map<std::string, std::string>& topics,
                     void (*publish_response_callback)(
                         void** state, struct mqtt_response_publish* publish)) {
  reconnect_state = new reconnect_state_t();
  reconnect_state->hostname = addr;
  reconnect_state->port = port;
  reconnect_state->username = username;
  reconnect_state->password = password;
  reconnect_state->clientName = client_name;
  auto mqttClient = Supla::LinuxMqttClient::getInstance();
  reconnect_state->useSSL = mqttClient->useSSL;
  reconnect_state->verifyCA = mqttClient->verifyCA;
  reconnect_state->fileCA = mqttClient->fileCA;

  for (const auto& topic : topics) {
    reconnect_state->topics[topic.first] = topic.second;
  }
  /* setup a client */
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  mq_client = new mqtt_client();
  // mq_client->protocol_version = protocol_version;

  mqtt_init_reconnect(
      mq_client, reconnect_client, reconnect_state, publish_response_callback);

  mqtt_loop_running = true;
  if (pthread_create(
          &mqtt_deamon_thread, nullptr, mqtt_client_loop, &mq_client)) {
    mqtt_loop_running = false;
    SUPLA_LOG_ERROR("Failed to start client daemon.");
    return EXIT_FAILURE;
  }
  SUPLA_LOG_DEBUG("Start MQTT client daemon.");
  return EXIT_SUCCESS;
}

void mqtt_client_publish(const char* topic,
                         const char* payload,
                         char retain,
                         char qos) {
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  if (mq_client == nullptr || mq_client->error != MQTT_OK) {
    return;
  }

  uint8_t publish_flags = 0;
  if (retain) {
    publish_flags |= MQTT_PUBLISH_RETAIN;
  }

  if (qos == 0) {
    publish_flags |= MQTT_PUBLISH_QOS_0;
  } else if (qos == 1) {
    publish_flags |= MQTT_PUBLISH_QOS_1;
  } else if (qos == 2) {
    publish_flags |= MQTT_PUBLISH_QOS_2;
  }

  SUPLA_LOG_DEBUG("publishing %s", topic);

  mqtt_publish(
      mq_client, topic, (const char*)payload, strlen(payload), publish_flags);
}

void reconnect_client(struct mqtt_client* client, void** reconnect_state_vptr) {
  struct reconnect_state_t* reconnect_state =
      *((struct reconnect_state_t**)reconnect_state_vptr);

  /* Close the clients socket if this isn't the initial reconnect call */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
    BIO* bio = reinterpret_cast<BIO*>(client->socketfd);
    if (bio != nullptr) {
      BIO_free_all(bio);
      client->socketfd = nullptr;
    }
  }

  /* Perform error handling here. */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
    SUPLA_LOG_ERROR("mqtt client error %s", mqtt_error_str(client->error));
    SUPLA_LOG_DEBUG("another connection attempt in %d s", delay_time);
    delay(delay_time * 1000);
    delay_time = (delay_time >= 60) ? 60 : delay_time + 15;
  } else {
    delay_time = 5;
  }

  SUPLA_LOG_DEBUG("connecting to MQTT broker %s on port %d",
                  reconnect_state->hostname.c_str(),
                  reconnect_state->port);

  if (!reconnect_state->clientName.empty()) {
    SUPLA_LOG_DEBUG("using client name %s",
                    reconnect_state->clientName.c_str());
  }

  if (!reconnect_state->username.empty()) {
    SUPLA_LOG_DEBUG("using credentials %s %s",
                    reconnect_state->username.c_str(),
                    reconnect_state->password.c_str());
  }

  /* Open a new socket. */
  void* sockfd = nullptr;
  try {
    sockfd = reinterpret_cast<void*>(open_supla_client(*reconnect_state));
  } catch (const std::runtime_error& e) {
    SUPLA_LOG_ERROR("An socket error occurred: %s", e.what());
  }
  if (sockfd == nullptr) {
    SUPLA_LOG_ERROR("socket error");
    sleep(5);
    client->error = MQTT_ERROR_INITIAL_RECONNECT;
    return;
  }

  /* Reinitialize the client. */
  mqtt_reinit(client,
              reinterpret_cast<BIO*>(sockfd),
              reconnect_state->sendbuf.data(),
              reconnect_state->sendbuf.size(),
              reconnect_state->recvbuf.data(),
              reconnect_state->recvbuf.size());

  const char* username = !reconnect_state->username.empty()
                             ? reconnect_state->username.c_str()
                             : nullptr;
  const char* password = !reconnect_state->password.empty()
                             ? reconnect_state->password.c_str()
                             : nullptr;
  const char* client_name = reconnect_state->clientName.c_str();

  /* Send connection request to the broker. */
  MQTTErrors connect = mqtt_connect(
      client, client_name, nullptr, nullptr, 0, username, password, 0, 400);

  if (connect == MQTT_OK) {
    for (const auto& topic : reconnect_state->topics) {
      SUPLA_LOG_DEBUG("subscribing \"%s\"", topic.first.c_str());
      mqtt_subscribe(client, topic.first.c_str(), 0);
    }
  }
}

void mqtt_client_free() {
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  if (mqtt_deamon_thread != 0) {
    mqtt_loop_running = false;
    pthread_cancel(mqtt_deamon_thread);
    pthread_join(mqtt_deamon_thread, nullptr);
    mqtt_deamon_thread = 0;
  }

  if (mq_client != nullptr) {
    mqtt_disconnect(mq_client);
  }

  if (mq_client != nullptr && mq_client->socketfd != nullptr) {
    BIO_free_all(reinterpret_cast<BIO*>(mq_client->socketfd));
    mq_client->socketfd = nullptr;
  }

  if (reconnect_state != nullptr) {
    delete reconnect_state;
    reconnect_state = nullptr;
  }

  delete mq_client;
  mq_client = nullptr;
}
