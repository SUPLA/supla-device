// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ELEMENT_WITH_STORAGE_H_
#define EXTRAS_TEST_DOUBLES_ELEMENT_WITH_STORAGE_H_

#include <supla/element.h>

class ElementWithStorage : public Supla::Element {
 public:
  ElementWithStorage();
  void onLoadState();
  void onSaveState();

  int32_t stateValue = -1;
};

#endif  // EXTRAS_TEST_DOUBLES_ELEMENT_WITH_STORAGE_H_
