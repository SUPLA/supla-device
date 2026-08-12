// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_PAYLOAD_JSON_H_
#define EXTRAS_PORTING_LINUX_SUPLA_PAYLOAD_JSON_H_
#include <supla/output/output.h>

#include <string>

#include "payload.h"

namespace Supla {
namespace Payload {
class Json : public Payload {
 public:
  explicit Json(Supla::Output::Output *);
  virtual ~Json();

  void turnOn(const std::string &key,
              std::variant<int, bool, std::string> onValue) override;
  void turnOff(const std::string &key,
               std::variant<int, bool, std::string> offValue) override;

  bool isBasedOnIndex() override;
};
}  // namespace Payload
}  // namespace Supla
#endif  // EXTRAS_PORTING_LINUX_SUPLA_PAYLOAD_JSON_H_
