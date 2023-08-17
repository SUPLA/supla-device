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

#include "correction.h"

using Supla::Correction;

void Supla::Correction::add(uint8_t channelNumber,
    double correction,
    bool forSecondaryValue) {
  auto ptr = getInstance(channelNumber, forSecondaryValue);
  if (ptr) {
    ptr->correction = correction;
  } else {
    new Correction(channelNumber, correction, forSecondaryValue);
  }
}

Correction *Correction::getInstance(uint8_t channelNumber,
                                    bool forSecondaryValue) {
  auto ptr = first;
  while (ptr) {
    if (ptr->channelNumber == channelNumber &&
        ptr->forSecondaryValue == forSecondaryValue) {
      return ptr;
    }
    ptr = ptr->next;
  }
  return nullptr;
}

double Supla::Correction::get(uint8_t channelNumber, bool forSecondaryValue) {
  auto ptr = getInstance(channelNumber, forSecondaryValue);
  if (ptr) {
    return ptr->correction;
  }

  return 0;
}

Supla::Correction::Correction(uint8_t channelNumber, double correction,
    bool forSecondaryValue)
  : next(nullptr),
  channelNumber(channelNumber),
  correction(correction),
  forSecondaryValue(forSecondaryValue) {
  if (first == nullptr) {
    first = this;
  } else {
    Correction *ptr = first;
    while (ptr && ptr->next) {
      ptr = ptr->next;
    }
    ptr->next = this;
  }
}

Supla::Correction::~Correction() {
  if (first == this) {
    first = next;
    return;
  }

  auto ptr = first;
  while (ptr->next != this) {
    ptr = ptr->next;
  }

  ptr->next = ptr->next->next;
}

void Supla::Correction::clear() {
  while (first) {
    delete first;
  }
}

Supla::Correction *Supla::Correction::first = nullptr;
