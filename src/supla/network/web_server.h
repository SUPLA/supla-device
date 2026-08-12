// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_WEB_SERVER_H_
#define SRC_SUPLA_NETWORK_WEB_SERVER_H_

#include <stddef.h>
#include <supla/network/html_generator.h>
#include <supla/network/html_element.h>
#include <supla/device/security_logger.h>
#include <stdint.h>

class SuplaDeviceClass;

#define HTML_KEY_LENGTH 16
#define HTML_VAL_LENGTH 4000

namespace Supla {

extern const unsigned char favico[1150];
constexpr size_t REDACTED_LOG_VALUE_BUFFER_SIZE = 32;

bool isSensitiveLogField(const char *key);
const char *redactLogValue(const char *key,
                           const char *value,
                           char *buffer,
                           size_t bufferSize);

class WebServer {
 public:
  enum class WebServerMode {
    HttpOnly,
    HttpsOnly,
    Auto,
  };

  static WebServer *Instance();
  explicit WebServer(Supla::HtmlGenerator *);
  virtual ~WebServer();
  virtual void start() = 0;
  virtual void stop() = 0;
  virtual void setWebServerMode(WebServerMode mode);
  virtual WebServerMode getWebServerMode() const;
  virtual WebServerMode resolveWebServerMode() const;
  void setSuplaDeviceClass(SuplaDeviceClass *);
  void notifyClientConnected(bool isPost = false);
  virtual void parsePost(const char *postContent,
                         int size,
                         bool lastChunk = true);
  virtual void resetParser();
  const char *getCsrfToken();
  bool isCsrfTokenValid(const char *token);
  void setBetaProcessing();
  void setCsrfFirstFieldRequired(bool required);
  void addLastStateLog(const char *log) const;

  virtual bool verifyEmbeddedHttpsCertificates();

  Supla::HtmlGenerator *htmlGenerator = nullptr;

 protected:
  void addSecurityLog(Supla::SecurityLogSource source, const char *log) const;
  void addSecurityLog(uint32_t source, const char *log) const;
  /**
   * Check if section is allowed in current POST request. It excludes
   * html elements in /beta page POST, and vice versa
   *
   * @param section
   *
   * @return true if section is allowed to be processed
   */
  bool isSectionAllowed(Supla::HtmlSection section) const;
  void cleanupParser();

  static WebServer *webServerInstance;
  bool destroyGenerator = false;
  SuplaDeviceClass *sdc = nullptr;
  bool keyFound = false;
  int partialSize = 0;
  char key[HTML_KEY_LENGTH] = {};
  char *value = nullptr;
  bool betaProcessing = false;
  bool csrfFirstFieldRequired = true;
  bool csrfValidated = false;
  bool csrfRejected = false;
  uint8_t pendingReboot = 0;
  uint8_t csrfSecret[16] = {};
  char csrfToken[33] = {};
};

};  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_WEB_SERVER_H_
