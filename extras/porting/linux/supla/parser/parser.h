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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_PARSER_PARSER_H_
#define EXTRAS_PORTING_LINUX_SUPLA_PARSER_PARSER_H_

#include <supla/source/source.h>

#include <map>
#include <string>
#include <cstdint>

namespace Supla {
namespace Parser {

class Parser {
 public:
  explicit Parser(Supla::Source::Source *);
  virtual ~Parser() {}
  bool refreshParserSource();

  virtual void addKey(const std::string &key, int index);
  virtual double getValue(const std::string &key) = 0;

  virtual bool isValid();
  virtual bool isBasedOnIndex() = 0;
  void setRefreshTime(unsigned int timeMs);

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
