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

#include "sw_update_mock.h"

#include <SuplaDevice.h>
#include <supla/device/sw_update.h>

SwUpdateMock *SwUpdateMock::instance = nullptr;

class SwUpdateFacade : public Supla::Device::SwUpdate {
 public:
  explicit SwUpdateFacade(SwUpdateMock *mock)
      : Supla::Device::SwUpdate(nullptr, nullptr), mock(mock) {
      }

  void iterate() override {
    mock->iterate();
  }

  void setFinished() {
    finished = true;
  }

  void setAborted() {
    abort = true;
  }

  void setNewVersion(const char *version) {
    if (newVersion) {
      delete[] newVersion;
      newVersion = nullptr;
    }
    newVersion = new char[strlen(version) + 1];
    strcpy(newVersion, version);
  }

  SwUpdateMock *mock;

};

Supla::Device::SwUpdate *Supla::Device::SwUpdate::Create(SuplaDeviceClass *,
                                                         const char *) {
  assert(SwUpdateMock::instance != nullptr);
  auto facade = new SwUpdateFacade(SwUpdateMock::instance);
  SwUpdateMock::instance->setFacade(facade);
  return facade;
}

SwUpdateMock::SwUpdateMock()
    : Supla::Device::SwUpdate(nullptr, nullptr) {
  instance = this;
}

void SwUpdateMock::setFinished() {
  facade->setFinished();
}

void SwUpdateMock::setAborted() {
  facade->setAborted();
}

void SwUpdateMock::setNewVersion(const char *version) {
  facade->setNewVersion(version);
}

bool SwUpdateMock::isSecurityOnlyOnFacade() {
  return facade->isSecurityOnly();
}
