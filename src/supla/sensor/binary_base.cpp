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

#include "binary_base.h"

#include <supla/time.h>

using Supla::Sensor::BinaryBase;

BinaryBase::BinaryBase() {
  channel.setType(SUPLA_CHANNELTYPE_SENSORNO);
}

BinaryBase::~BinaryBase() {
}

void BinaryBase::iterateAlways() {
  if (millis() - lastReadTime > readIntervalMs) {
    lastReadTime = millis();
    channel.setNewValue(getValue());
  }
}

Supla::Channel *BinaryBase::getChannel() {
  return &channel;
}

void BinaryBase::setServerInvertLogic(bool invertLogic) {
  channel.setServerInvertLogic(invertLogic);
}

void BinaryBase::setReadIntervalMs(uint32_t intervalMs) {
  if (intervalMs == 0) {
    intervalMs = 100;
  }
  readIntervalMs = intervalMs;
}
