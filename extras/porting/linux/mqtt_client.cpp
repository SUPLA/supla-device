/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#include "mqtt_client.h"

#include <pthread.h>
#include <supla/time.h>
#include <tools.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>

#include "linux_mqtt_client.h"

pthread_t mqtt_deamon_thread = 0;
struct reconnect_state_t* reconnect_state;

void reconnect_client(struct mqtt_client* client, void** reconnect_state_vptr);

#if defined(MQTT_USE_BIO)

#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/ssl.h>

SSL_CTX* ssl_ctx;

BIO* open_nb_socket(const char* addr, const char* port) {
  // Address and port definitions
  char* addr_copy = (char*)malloc(strlen(addr) + 1);
  strcpy(addr_copy, addr);
  char* port_copy = (char*)malloc(strlen(port) + 1);
  strcpy(port_copy, port);

  // Default behaviour (Assuming non-encrypted connection)
  BIO* bio = BIO_new_connect(addr_copy);
  BIO_set_nbio(bio, 1);
  BIO_set_conn_port(bio, port_copy);
  free(addr_copy);
  free(port_copy);

  std::shared_ptr<Supla::LinuxMqttClient>& mqttClient =
      Supla::LinuxMqttClient::getInstance();
  bool useSSL = mqttClient->useSSL;
  bool verifyCA = mqttClient->verifyCA;
  std::string fileCA = mqttClient->fileCA;

  // Check if SSL encryption is configured
  if (useSSL) {
    ssl_ctx = SSL_CTX_new(SSLv23_client_method());
    SSL* ssl;

    /* load certificate */
    if (verifyCA) {
      if (fileCA.empty()) {
        if (!SSL_CTX_set_default_verify_paths(ssl_ctx)) {
          throw std::runtime_error("failed to load system certificate path");
        }
      } else {
        if (!SSL_CTX_load_verify_locations(ssl_ctx, fileCA.c_str(), nullptr)) {
          throw std::runtime_error("failed to load ca certificate");
        }
      }
    }
    /* Upgrade the BIO to SSL connection */
    BIO* ssl_bio = BIO_new_ssl(ssl_ctx, 1);
    bio = BIO_push(ssl_bio, bio);

    /* wait for connect with 10 second timeout */
    int start_time = (int)time(nullptr);
    int do_connect_rv = (int)BIO_do_connect(bio);
    while (do_connect_rv <= 0 && BIO_should_retry(bio) &&
           (int)time(nullptr) - start_time < 10) {
      do_connect_rv = (int)BIO_do_connect(bio);
    }
    if (do_connect_rv <= 0) {
      SUPLA_LOG_ERROR("%s", ERR_reason_error_string(ERR_get_error()));
      BIO_free_all(bio);
      SSL_CTX_free(ssl_ctx);
      ssl_ctx = nullptr;
      return nullptr;
    }

    BIO_get_ssl(bio, &ssl);
    X509* cert = SSL_get_peer_certificate(ssl);
    if (cert) {
      char* line;
      SUPLA_LOG_DEBUG("Certificate MQTT broker:");
      line = X509_NAME_oneline(X509_get_subject_name(cert), 0, 0);
      SUPLA_LOG_DEBUG("Subject: %s", line);
      free(line);
      line = X509_NAME_oneline(X509_get_issuer_name(cert), 0, 0);
      SUPLA_LOG_DEBUG("Issuer: %s", line);
      free(line);
      X509_free(cert);
    }
    if (verifyCA) {
      if (SSL_get_verify_result(ssl) == X509_V_OK) {
        SUPLA_LOG_DEBUG("x509 certificate verification successful");
      } else {
        throw std::runtime_error("x509 certificate verification failed");
      }
    } else {
      SUPLA_LOG_WARNING(
          "Connecting to MQTT broker without certificate validation "
          "(INSECURE)");
    }
  }
  return bio;
}

#else

int open_nb_socket(const char* addr, uint16_t port) {
  struct addrinfo hints = {0};

  hints.ai_family = AF_UNSPEC;     /* IPv4 or IPv6 */
  hints.ai_socktype = SOCK_STREAM; /* Must be TCP */
  int sockfd = -1;
  int rv;
  struct addrinfo *p, *servinfo;

  char port_buffer[6];
  sprintf(port_buffer, "%d", port);

  /* get address information */
  rv = getaddrinfo(addr, port_buffer, &hints, &servinfo);
  if (rv != 0) {
    SUPLA_LOG_ERROR("Failed to open socket (getaddrinfo): %s",
                    gai_strerror(rv));
    return -1;
  }
  SUPLA_LOG_DEBUG("Open socket.");

  /* open the first possible socket */
  for (p = servinfo; p != nullptr; p = p->ai_next) {
    sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
    if (sockfd == -1) continue;

    /* connect to server */
    rv = connect(sockfd, servinfo->ai_addr, servinfo->ai_addrlen);
    if (rv == -1) continue;
    SUPLA_LOG_DEBUG("Connect socket %d %d", sockfd, rv);
    break;
  }

  /* free servinfo */
  freeaddrinfo(servinfo);

  /* make non-blocking */
  if (sockfd != -1) {
    fcntl(sockfd, F_SETFL, fcntl(sockfd, F_GETFL) | O_NONBLOCK);
  }
  SUPLA_LOG_DEBUG("Open socket non-blocking %d", sockfd);
  /* return the new socket fd */
  return sockfd;
}
#endif

void* mqtt_client_loop(void* client) {
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  SUPLA_LOG_DEBUG("Start MQTT client loop...");
  while (st_app_terminate == 0) {
    mqtt_sync((struct mqtt_client*)mq_client);
    delay(100);
  }
  SUPLA_LOG_DEBUG("Stop MQTT client loop.");
  return nullptr;
}

void close_client(int sockfd, pthread_t* client_daemon) {
#if defined(MQTT_USE_BIO)
  BIO* bio = (BIO*)(intptr_t)sockfd;
  BIO_free_all(bio);
#else
  close(sockfd);
#endif
  if (client_daemon != nullptr) pthread_cancel(*client_daemon);
}

int mqtt_client_init(std::string addr,
                     int port,
                     std::string username,
                     std::string password,
                     std::string client_name,
                     uint8_t protocol_version,
                     std::unordered_map<std::string, std::string>& topics,
                     void (*publish_response_callback)(
                         void** state, struct mqtt_response_publish* publish)) {
  reconnect_state = new reconnect_state_t();
  reconnect_state->sendbuf = (uint8_t*)malloc(8192 * sizeof(uint8_t));
  reconnect_state->sendbufsz = 8192 * sizeof(uint8_t);
  reconnect_state->recvbuf = (uint8_t*)malloc(2048 * sizeof(uint8_t));
  reconnect_state->recvbufsz = 2048 * sizeof(uint8_t);
  reconnect_state->hostname = addr;
  reconnect_state->port = port;
  reconnect_state->username = username;
  reconnect_state->password = password;
  reconnect_state->clientName = client_name;

  for (const auto& topic : topics) {
    reconnect_state->topics[topic.first] = topic.second;
  }
  /* setup a client */
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  mq_client = new mqtt_client();
  // mq_client->protocol_version = protocol_version;

  mqtt_init_reconnect(
      mq_client, reconnect_client, reconnect_state, publish_response_callback);

  if (pthread_create(
          &mqtt_deamon_thread, nullptr, mqtt_client_loop, &mq_client)) {
    SUPLA_LOG_ERROR("Failed to start client daemon.");
  }
  SUPLA_LOG_DEBUG("Start MQTT client daemon.");
  return EXIT_SUCCESS;
}

void mqtt_client_publish(const char* topic,
                         const char* payload,
                         char retain,
                         char qos) {
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  if (mq_client == nullptr || mq_client->error != MQTT_OK) return;

  uint8_t publish_flags = 0;
  if (retain) {
    publish_flags |= MQTT_PUBLISH_RETAIN;
  }

  if (qos == 0)
    publish_flags |= MQTT_PUBLISH_QOS_0;
  else if (qos == 1)
    publish_flags |= MQTT_PUBLISH_QOS_1;
  else if (qos == 2)
    publish_flags |= MQTT_PUBLISH_QOS_2;

  SUPLA_LOG_DEBUG("publishing %s", topic);

  mqtt_publish(
      mq_client, topic, (void*)payload, strlen(payload), publish_flags);
}

void reconnect_client(struct mqtt_client* client, void** reconnect_state_vptr) {
  struct reconnect_state_t* reconnect_state =
      *((struct reconnect_state_t**)reconnect_state_vptr);

  /* Close the clients socket if this isn't the initial reconnect call */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
#if defined(MQTT_USE_BIO)
    BIO* bio = (BIO*)client->socketfd;
    BIO_free_all(bio);
    SSL_CTX_free(ssl_ctx);
#else
    close(client->socketfd);
#endif
  }

  /* Perform error handling here. */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
    SUPLA_LOG_ERROR("mqtt client error %s", mqtt_error_str(client->error));
    // sleep(5);
  }

  SUPLA_LOG_DEBUG("connecting to MQTT broker %s on port %d",
                  reconnect_state->hostname.c_str(),
                  reconnect_state->port);

  if (reconnect_state->username.length() > 0) {
    SUPLA_LOG_DEBUG("using credentials %s %s",
                    reconnect_state->username.c_str(),
                    reconnect_state->password.c_str());
  }

  /* Open a new socket. */
  void* sockfd = nullptr;
  try {
#if defined(MQTT_USE_BIO)
    sockfd = (void*)open_nb_socket(reconnect_state->hostname.c_str(),
                                   to_string(reconnect_state->port).c_str());
#else
    sockfd = (void*)(intptr_t)open_nb_socket(reconnect_state->hostname.c_str(),
                                             reconnect_state->port);
#endif
  } catch (const std::runtime_error& e) {
    SUPLA_LOG_ERROR("An socket error occurred: %s", e.what());
  }
  if (sockfd == nullptr) {
    SUPLA_LOG_ERROR("socket error");
    sleep(5);
    client->error = MQTT_ERROR_INITIAL_RECONNECT;
    return;
  }

  /* Reinitialize buffers */
  if (client->error != MQTT_ERROR_INITIAL_RECONNECT) {
    free(reconnect_state->sendbuf);
    free(reconnect_state->recvbuf);

    reconnect_state->sendbuf = (uint8_t*)malloc(8192 * sizeof(uint8_t));
    reconnect_state->sendbufsz = 8192 * sizeof(uint8_t);
    reconnect_state->recvbuf = (uint8_t*)malloc(2048 * sizeof(uint8_t));
    reconnect_state->recvbufsz = 2048 * sizeof(uint8_t);
  }

  /* Reinitialize the client. */
#if defined(MQTT_USE_BIO)
  mqtt_reinit(client,
              (BIO*)sockfd,
              reconnect_state->sendbuf,
              reconnect_state->sendbufsz,
              reconnect_state->recvbuf,
              reconnect_state->recvbufsz);
#else
  mqtt_reinit(client,
              (int)(intptr_t)sockfd,
              reconnect_state->sendbuf,
              reconnect_state->sendbufsz,
              reconnect_state->recvbuf,
              reconnect_state->recvbufsz);
#endif

  const char* username = !reconnect_state->username.empty()
                             ? reconnect_state->username.c_str()
                             : nullptr;
  const char* password = !reconnect_state->password.empty()
                             ? reconnect_state->password.c_str()
                             : nullptr;
  const char* client_name = reconnect_state->clientName.c_str();

  /* Send connection request to the broker. */
  mqtt_connect(
      client, client_name, nullptr, nullptr, 0, username, password, 0, 400);

  /* Subscribe to the topic. */
  for (const auto& topic : reconnect_state->topics) {
    SUPLA_LOG_DEBUG("subscribing %s", topic.first.c_str());
    mqtt_subscribe(client, topic.first.c_str(), 0);
  }
}

void mqtt_client_free() {
  auto& mq_client = Supla::LinuxMqttClient::getInstance()->mq_client;
  if (mq_client != nullptr) mqtt_disconnect(mq_client);

  if (mqtt_deamon_thread != 0) {
    pthread_cancel(mqtt_deamon_thread);
  }

#if defined(MQTT_USE_BIO)
  void* sockfd = (void*)mq_client->socketfd;
  BIO* bio = (BIO*)sockfd;
  BIO_free_all(bio);
  SSL_CTX_free(ssl_ctx);
#else
  if (mq_client != nullptr && mq_client->socketfd != -1) {
    close(mq_client->socketfd);
  }
#endif

  if (reconnect_state != nullptr) {
    free(reconnect_state->recvbuf);
    free(reconnect_state->sendbuf);
    delete reconnect_state;
  }

  delete mq_client;
}
