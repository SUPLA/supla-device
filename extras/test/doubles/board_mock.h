// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_BOARD_MOCK_H_
#define EXTRAS_TEST_DOUBLES_BOARD_MOCK_H_

#include <gmock/gmock.h>

class BoardInterface {
 public:
  BoardInterface();
  virtual ~BoardInterface();

  static BoardInterface *instance;

  virtual void deviceSoftwareReset() = 0;
};

class BoardMock : public BoardInterface {
 public:
  BoardMock();
  virtual ~BoardMock();
  MOCK_METHOD(void, deviceSoftwareReset, (), (override));
};

void setLastResetSoft(bool value);
void setDeviceSoftwareResetSupported(bool value);

#endif  // EXTRAS_TEST_DOUBLES_BOARD_MOCK_H_
