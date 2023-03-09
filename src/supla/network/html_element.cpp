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
