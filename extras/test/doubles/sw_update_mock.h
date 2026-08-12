// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_SW_UPDATE_MOCK_H_
#define EXTRAS_TEST_DOUBLES_SW_UPDATE_MOCK_H_

#include <supla/device/sw_update.h>

#include <gmock/gmock.h>

class SwUpdateFacade;

class SwUpdateMock : public Supla::Device::SwUpdate {
 public:
  explicit SwUpdateMock();
  static SwUpdateMock *instance;
  SwUpdateFacade *facade;

  MOCK_METHOD(void, iterate, (), (override));

  void setFinished();
  void setAborted();
  void setFacade(SwUpdateFacade *facade) { this->facade = facade; }
  void setNewVersion(const char *version);
  bool isSecurityOnlyOnFacade();
  bool isSkipCertOnFacade();
};


#endif  // EXTRAS_TEST_DOUBLES_SW_UPDATE_MOCK_H_
