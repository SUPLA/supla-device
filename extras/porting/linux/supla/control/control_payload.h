// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CONTROL_PAYLOAD_H_
#define EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CONTROL_PAYLOAD_H_

#include <supla/payload/payload.h>

#include <string>
#include <variant>
#include <map>

namespace Supla {
namespace Payload {

class ControlPayloadBase {
 public:
  explicit ControlPayloadBase(Supla::Payload::Payload*);

  void setMapping(const std::string &parameter, const std::string &key);
  void setMapping(const std::string &parameter, const int index);

  void setSetOnValue(const std::variant<int, bool, std::string>& value);
  void setSetOffValue(const std::variant<int, bool, std::string>& value);

 protected:
  // payload configuration
  int id;
  Supla::Payload::Payload* payload = nullptr;
  std::map<std::string, std::string> parameter2Key;
  std::variant<int, bool, std::string> setOnValue;
  std::variant<int, bool, std::string> setOffValue;
};

template <typename T>
class ControlPayload : public T, public ControlPayloadBase {
 public:
  explicit ControlPayload(Supla::Payload::Payload *);
};

template <typename T>
ControlPayload<T>::ControlPayload(Supla::Payload::Payload *payload)
    : ControlPayloadBase(payload) {  // NOLINT
}

}  // namespace Payload
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_CONTROL_CONTROL_PAYLOAD_H_
