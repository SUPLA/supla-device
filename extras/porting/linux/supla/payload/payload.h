// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_PAYLOAD_PAYLOAD_H_
#define EXTRAS_PORTING_LINUX_SUPLA_PAYLOAD_PAYLOAD_H_

#include <supla/output/output.h>

#include <map>
#include <string>
#include <variant>

namespace Supla {
namespace Payload {

class Payload {
 public:
  explicit Payload(Supla::Output::Output *);
  virtual ~Payload() {
  }

  virtual void addKey(const std::string &key, int index);
  virtual bool isBasedOnIndex() = 0;

  virtual void turnOn(const std::string &key,
                      std::variant<int, bool, std::string> onValue) = 0;

  virtual void turnOff(const std::string &key,
                       std::variant<int, bool, std::string> offValue) = 0;

 protected:
  std::map<std::string, int> keys;
  Supla::Output::Output *output = nullptr;
};
};  // namespace Payload
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_PAYLOAD_PAYLOAD_H_
