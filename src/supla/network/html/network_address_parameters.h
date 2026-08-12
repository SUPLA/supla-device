// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_NETWORK_HTML_NETWORK_ADDRESS_PARAMETERS_H_
#define SRC_SUPLA_NETWORK_HTML_NETWORK_ADDRESS_PARAMETERS_H_

#include <supla/network/netif_config.h>

class SuplaDeviceClass;

namespace Supla {

class WebSender;

namespace Html {

class NetworkAddressParameters {
 public:
  NetworkAddressParameters(const char* blobName, const char* prefix);

  void send(Supla::WebSender* sender);
  bool handleResponse(const char* key, const char* value);
  void onProcessingEnd();

 private:
  void ensureLoaded();
  void renderField(Supla::WebSender* sender,
                   const char* suffix,
                   const char* label,
                   const char* value,
                   const char* placeholder,
                   const char* pattern,
                   const char* title,
                   bool requiredWhenStatic);
  bool matchKey(const char* key, const char* suffix) const;
  void buildKey(char* out, size_t outSize, const char* suffix) const;
  void buildId(char* out, size_t outSize, const char* suffix) const {
    buildKey(out, outSize, suffix);
  }

  const char* blobName_ = nullptr;
  const char* prefix_ = nullptr;
  const char* sectionTitle_ = nullptr;
  NetifConfigBlob config_ = {};
  bool loaded_ = false;
  bool responseSeen_ = false;
  bool invalidInput_ = false;
};

}  // namespace Html
}  // namespace Supla

#endif  // SRC_SUPLA_NETWORK_HTML_NETWORK_ADDRESS_PARAMETERS_H_
