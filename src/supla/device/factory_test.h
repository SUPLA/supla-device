/*
 * Copyright (C) AC SOFTWARE SP. Z O.O
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 */

#ifndef SRC_SUPLA_DEVICE_FACTORY_TEST_H_
#define SRC_SUPLA_DEVICE_FACTORY_TEST_H_

#include <supla/action_handler.h>
#include <supla/element.h>

class SuplaDeviceClass;

namespace Supla {

enum TestStage {
  TestStage_None = 0,
  TestStage_Init = 1,
  TestStage_RegisteredAndReady = 2,
  TestStage_WaitForCfgButton = 3,
  TestStage_Failed = 4,
  TestStage_Success = 5
};

enum TestActionType {
  TestAction_CfgButtonOnPress = 11000,
  TestAction_DeviceStatusChange = 11101
};

namespace Device {

class FactoryTest : public Supla::ActionHandler, public Supla::Element {
 public:
  FactoryTest(SuplaDeviceClass *sdc, uint32_t timeoutS);
  virtual ~FactoryTest();

  void onInit() override;
  void iterateAlways() override;
  void handleAction(int event, int action) override;

  virtual int16_t getManufacturerId();
  virtual void waitForConfigButtonPress();
  virtual bool checkTestStep();

  void setTestFailed(int reason);

 protected:
  bool testFailed = false;
  bool testFinished = false;
  bool waitForRegisteredAndReady = true;
  bool testingMachineEnabled = true;
  SuplaDeviceClass *sdc = nullptr;
  uint32_t timeoutS = 30;
  uint32_t initTimestamp = 0;
  Supla::TestStage testStage = Supla::TestStage_None;
  int testStep = 0;
  // fill fail reason for devices that can show it during the test
  // Use values >= 100 for device specific failures
  int failReason = 0;
};

}  // namespace Device
}  // namespace Supla
#endif  // SRC_SUPLA_DEVICE_FACTORY_TEST_H_
