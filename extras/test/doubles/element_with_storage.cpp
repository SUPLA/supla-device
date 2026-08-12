// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "element_with_storage.h"

#include <supla/storage/storage.h>

ElementWithStorage::ElementWithStorage() {
}

void ElementWithStorage::onLoadState() {
  Supla::Storage::ReadState(reinterpret_cast<unsigned char *>(&stateValue),
      sizeof(stateValue));
}

void ElementWithStorage::onSaveState() {
  Supla::Storage::WriteState(reinterpret_cast<unsigned char *>(&stateValue),
      sizeof(stateValue));
}

