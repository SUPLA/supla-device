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

#include "virtual_binary.h"

#include <supla/actions.h>
#include <supla/storage/storage.h>
#include <supla/time.h>

namespace Supla {
namespace Sensor {

VirtualBinary::VirtualBinary(bool keepStateInStorage)
    : keepStateInStorage(keepStateInStorage) {
}

void VirtualBinary::setKeepStateInStorage(bool keepStateInStorage) {
  this->keepStateInStorage = keepStateInStorage;
}

void VirtualBinary::setUseConfiguredTimeout(bool useConfiguredTimeout) {
  this->useConfiguredTimeout = useConfiguredTimeout;
}

bool VirtualBinary::getValue() {
  return state;
}

void VirtualBinary::onInit() {
  clearedByTimeout = false;
  channel.setNewValue(getValue());
  stateChangeTimeMs = millis();
}

void VirtualBinary::iterateAlways() {
  if (useConfiguredTimeout) {
    uint16_t timeoutDs = getTimeoutDs();
    if (timeoutDs > 0 && channel.getValueBool()) {
      uint32_t timeoutMs = static_cast<uint32_t>(timeoutDs) * 100;
      if (millis() - stateChangeTimeMs > timeoutMs) {
        setLogicalState(false, true);
      }
    }
  }
  BinaryBase::iterateAlways();
}

void VirtualBinary::onSaveState() {
  if (keepStateInStorage) {
    Supla::Storage::WriteState((unsigned char *)&state, sizeof(state));
  }
}

void VirtualBinary::onLoadState() {
  if (keepStateInStorage) {
    bool value = false;
    if (Supla::Storage::ReadState((unsigned char *)&value, sizeof(value))) {
      state = value;
    }
  }
}

void VirtualBinary::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case SET: {
      set();
      break;
    }
    case CLEAR: {
      clear();
      break;
    }
    case TOGGLE: {
      toggle();
      break;
    }
  }
}

void VirtualBinary::set() {
  state = true;
  stateChangeTimeMs = millis();
  clearedByTimeout = false;
}

void VirtualBinary::clear() {
  state = false;
  stateChangeTimeMs = millis();
  clearedByTimeout = false;
}

void VirtualBinary::toggle() {
  state = !state;
  stateChangeTimeMs = millis();
  clearedByTimeout = false;
}

void VirtualBinary::setLogicalState(bool logicalState, bool fromTimeout) {
  if (channel.isServerInvertLogic() == logicalState) {
    clear();
  } else {
    set();
  }
  clearedByTimeout = fromTimeout;
}

};  // namespace Sensor
};  // namespace Supla
