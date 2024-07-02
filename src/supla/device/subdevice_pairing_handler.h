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

#ifndef SRC_SUPLA_DEVICE_SUBDEVICE_PAIRING_HANDLER_H_
#define SRC_SUPLA_DEVICE_SUBDEVICE_PAIRING_HANDLER_H_

#include <stdint.h>
#include <supla-common/proto.h>

namespace Supla {
namespace Protocol {
class SuplaSrpc;
}  // namespace Protocol

namespace Device {

class SubdevicePairingHandler {
 public:
  virtual ~SubdevicePairingHandler() = default;

  virtual bool startPairing(Supla::Protocol::SuplaSrpc *srpc,
                            TCalCfg_SubdevicePairingResult *result) = 0;
};

}  // namespace Device
}  // namespace Supla


#endif  // SRC_SUPLA_DEVICE_SUBDEVICE_PAIRING_HANDLER_H_
