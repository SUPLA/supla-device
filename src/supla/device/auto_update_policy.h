/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#ifndef SRC_SUPLA_DEVICE_AUTO_UPDATE_POLICY_H_
#define SRC_SUPLA_DEVICE_AUTO_UPDATE_POLICY_H_

#include <stdint.h>

namespace Supla {
enum class AutoUpdatePolicy: uint8_t {
  ForcedOff = 0,
  Disabled = 1,
  SecurityOnly = 2,  // default
  AllUpdates = 3
};

}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_AUTO_UPDATE_POLICY_H_
