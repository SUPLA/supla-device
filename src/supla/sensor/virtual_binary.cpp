// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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
  setInitialChannelValue(getValue());
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
