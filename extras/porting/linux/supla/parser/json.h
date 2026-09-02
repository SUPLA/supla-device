// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
  bool isSourceValid() override;

 protected:
  bool valid = false;
  bool sourceValid = false;
  std::map<std::string, int> keys;

  nlohmann::json json;
};
};  // namespace Parser
};  // namespace Supla
#endif  // EXTRAS_PORTING_LINUX_SUPLA_PARSER_JSON_H_
