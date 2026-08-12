// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef EXTRAS_TEST_DOUBLES_ELEMENT_MOCK_H_
#define EXTRAS_TEST_DOUBLES_ELEMENT_MOCK_H_

#include <gmock/gmock.h>
#include <supla/element.h>

class ElementMock : public Supla::Element {
 public:
  ElementMock();
  virtual ~ElementMock();
  MOCK_METHOD(void, onInit, (), (override));
  MOCK_METHOD(void, onLoadConfig, (SuplaDeviceClass *), (override));
  MOCK_METHOD(void, onLoadState, (), (override));
  MOCK_METHOD(void, onSaveState, (), (override));
  MOCK_METHOD(void, onRegistered, (Supla::Protocol::SuplaSrpc *), (override));
  MOCK_METHOD(void, iterateAlways, (), (override));
  MOCK_METHOD(bool, iterateConnected, (), (override));
  MOCK_METHOD(void, onTimer, (), (override));
  MOCK_METHOD(void, onFastTimer, (), (override));
  MOCK_METHOD(int,
              handleNewValueFromServer,
              (TSD_SuplaChannelNewValue *),
              (override));
  MOCK_METHOD(void, handleGetChannelState, (TDSC_ChannelState *), (override));
  MOCK_METHOD(int,
              handleCalcfgFromServer,
              (TSD_DeviceCalCfgRequest *),
              (override));
};

#endif  // EXTRAS_TEST_DOUBLES_ELEMENT_MOCK_H_
