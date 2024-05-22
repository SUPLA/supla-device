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

#ifndef EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_CMD_H_
#define EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_CMD_H_

#include <string>
#include <vector>

#include "output.h"

namespace Supla {
namespace Output {

class Cmd : public Output {
 public:
  explicit Cmd(const char *cmd);
  virtual ~Cmd();

 protected:
  std::string cmdLine;

 private:
  bool putContent(int payload) override;
  bool putContent(const std::string &payload) override;
  bool putContent(const std::vector<int> &payload) override;
  bool putContent(bool payload) override;
};

}  // namespace Output
}  // namespace Supla

#endif  // EXTRAS_PORTING_LINUX_SUPLA_OUTPUT_CMD_H_
