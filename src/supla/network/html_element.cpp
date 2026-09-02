// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "html_element.h"

namespace Supla {

HtmlElement *HtmlElement::firstPtr = nullptr;

HtmlElement::HtmlElement(HtmlSection section) : section(section) {
  if (firstPtr == nullptr) {
    firstPtr = this;
  } else {
    last()->nextPtr = this;
  }
}

HtmlElement::~HtmlElement() {
  if (begin() == this) {
    firstPtr = next();
    return;
  }

  auto ptr = begin();
  while (ptr->next() != this) {
    ptr = ptr->next();
  }

  ptr->nextPtr = ptr->next()->next();
}

HtmlElement *HtmlElement::begin() {
  return firstPtr;
}

HtmlElement *HtmlElement::last() {
  auto ptr = firstPtr;
  while (ptr && ptr->nextPtr) {
    ptr = ptr->nextPtr;
  }
  return ptr;
}

HtmlElement *HtmlElement::next() {
  return nextPtr;
}

const char *HtmlElement::selected(bool isSelected) {
  return isSelected ? " selected" : "";
}

const char *HtmlElement::checked(bool isChecked) {
  return isChecked ? " checked" : "";
}

bool HtmlElement::handleResponse(const char *key, const char *value) {
  (void)(key);
  (void)(value);
  return false;
}

void HtmlElement::onProcessingEnd() {
}

};  // namespace Supla
