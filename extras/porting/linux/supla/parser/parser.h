// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_PORTING_LINUX_SUPLA_PARSER_PARSER_H_
#define EXTRAS_PORTING_LINUX_SUPLA_PARSER_PARSER_H_

#include <supla/source/source.h>

#include <cstdint>
#include <map>
#include <string>
#include <variant>

namespace Supla {
namespace Parser {

class Parser {
 public:
  explicit Parser(Supla::Source::Source *);
  virtual ~Parser() {
  }
  bool refreshParserSource();

  virtual void addKey(const std::string &key, int index);
  virtual double getValue(const std::string &key) = 0;
  virtual std::variant<int, bool, std::string> getStateValue(
      const std::string &key) = 0;

  virtual bool isValid();
  virtual bool isSourceValid();
  virtual bool isBasedOnIndex() = 0;
  void setRefreshTime(unsigned int timeMs);
  bool isSourceConnected() const;

 protected:
  virtual bool refreshSource() = 0;
  std::map<std::string, int> keys;
  bool valid = false;
  Supla::Source::Source *source = nullptr;
  uint32_t lastRefreshTime = 0;
  unsigned int refreshTimeMs = 5 * 1000;  // 5 s
};
};  // namespace Parser
};  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_PARSER_PARSER_H_
