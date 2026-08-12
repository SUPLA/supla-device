// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

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

Supla::Correction::Correction(uint8_t channelNumber,
                              double correction,
                              bool forSecondaryValue)
    : correction(correction),
      channelNumber(channelNumber),
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
