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
