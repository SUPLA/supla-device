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

#ifndef SRC_SUPLA_CHANNELS_BINARY_SENSOR_CHANNEL_H_
#define SRC_SUPLA_CHANNELS_BINARY_SENSOR_CHANNEL_H_

#include "channel.h"

namespace Supla {
class BinarySensorChannel : public Channel {
 public:
  bool getValueBool() override;
  void setServerInvertLogic(bool invert);
  bool isServerInvertLogic() const;

 protected:
  // serverInvertLogic does not affect state reported to Supla server. However
  // it may be used internally by device in order to respect server settings.
  // Also external integrations (i.e. MQTT) use it to report inverted logic.
  // getValueBool returns value with serverInvertLogic applied.
  bool serverInvertLogic = false;
};

}  // namespace Supla

#endif  // SRC_SUPLA_CHANNELS_BINARY_SENSOR_CHANNEL_H_
