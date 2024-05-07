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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_PARSER_JSON_H_
#define EXTRAS_PORTING_LINUX_SUPLA_PARSER_JSON_H_

#include <supla/source/source.h>

#include <map>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

#include "parser.h"

namespace Supla {
namespace Parser {
class Json : public Parser {
 public:
  explicit Json(Supla::Source::Source *);
  virtual ~Json();

  bool refreshSource() override;

  double getValue(const std::string &key) override;
  std::variant<int, bool, std::string> getStateValue(
      const std::string &key) override;

  bool isBasedOnIndex() override;
  bool isValid() override;

 protected:
  bool valid = false;
  std::map<std::string, int> keys;

  nlohmann::json json;
};
};  // namespace Parser
};  // namespace Supla
#endif  // EXTRAS_PORTING_LINUX_SUPLA_PARSER_JSON_H_
