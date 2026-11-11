// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_PARSER_SIMPLE_H_
#define EXTRAS_PORTING_LINUX_SUPLA_PARSER_SIMPLE_H_

#include <supla/source/source.h>

#include <string>
#include <vector>

#include "parser.h"

namespace Supla {

namespace Parser {
class Simple : public Parser {
 public:
  explicit Simple(Supla::Source::Source *);
  virtual ~Simple();

  bool isBasedOnIndex() override;
  bool refreshSource() override;

  double getValue(const std::string &key) override;
  std::variant<int, bool, std::string> getStateValue(
      const std::string &key) override;

 protected:
  static constexpr size_t MAX_FIELDS = 4096;
  std::vector<std::variant<int, bool, std::string, double>> values;
};
};  // namespace Parser
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_PARSER_SIMPLE_H_
