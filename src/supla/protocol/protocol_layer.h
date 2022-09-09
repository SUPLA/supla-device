/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SRC_SUPLA_PROTOCOL_PROTOCOL_LAYER_H_
#define SRC_SUPLA_PROTOCOL_PROTOCOL_LAYER_H_

#include <stdint.h>

class SuplaDeviceClass;

namespace Supla {

namespace Protocol {

class ProtocolLayer {
 public:
  explicit ProtocolLayer(SuplaDeviceClass *sdc);
  virtual ~ProtocolLayer();
  static ProtocolLayer *first();
  static ProtocolLayer *last();
  ProtocolLayer *next();
  SuplaDeviceClass *getSdc();

  virtual void onInit() = 0;
  virtual bool onLoadConfig() = 0;
  virtual bool verifyConfig() = 0;
  virtual bool isEnabled() = 0;
  virtual void disconnect() = 0;
  virtual void iterate(uint64_t _millis) = 0;
  virtual bool isNetworkRestartRequested() = 0;
  virtual uint32_t getConnectionFailTime() = 0;
  virtual bool isConnectionError();
  virtual bool isConnecting();

 protected:
  static ProtocolLayer *firstPtr;
  ProtocolLayer *nextPtr = nullptr;
  SuplaDeviceClass *sdc = nullptr;
};

}  // namespace Protocol
}  // namespace Supla

#endif  // SRC_SUPLA_PROTOCOL_PROTOCOL_LAYER_H_
