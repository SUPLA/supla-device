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

#include <SuplaDevice.h>
#include <supla/log_wrapper.h>
#include <supla/time.h>
#include <stdio.h>
#include <string.h>
#include <supla/actions.h>

#include "factory_test.h"

namespace Supla {
namespace Device {

FactoryTest::FactoryTest(SuplaDeviceClass *sdc, uint32_t timeoutS)
    : sdc(sdc), timeoutS(timeoutS) {
}

FactoryTest::~FactoryTest() {
}

int16_t FactoryTest::getManufacturerId() {
  return -1;
}

void FactoryTest::onInit() {
  initTimestamp = millis();

  if (testStage != Supla::TestStage_None || testStep != 0) {
    SUPLA_LOG_ERROR(
        "TEST[%d,%d] failed: invalid step/stage sequence", testStage, testStep);
    testFailed = true;
    return;
  }

  testStage = Supla::TestStage_Init;

  if (Supla::Channel::reg_dev.ManufacturerID != getManufacturerId()) {
    SUPLA_LOG_ERROR("TEST failed: invalid ManufacturerID");
    testFailed = true;
    failReason = 1;
    return;
  }

  if (Supla::Channel::reg_dev.ProductID == 0) {
    SUPLA_LOG_ERROR("TEST failed: ProductID is empty");
    testFailed = true;
    failReason = 2;
    return;
  }

  if (strnlen(Supla::Channel::reg_dev.Name, SUPLA_DEVICE_NAME_MAXSIZE) == 0) {
    SUPLA_LOG_ERROR("TEST failed: device name is empty");
    testFailed = true;
    failReason = 3;
    return;
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing Config instance");
    testFailed = true;
    failReason = 4;
    return;
  }

  if (!sdc->getStorageInitResult()) {
    SUPLA_LOG_ERROR("TEST failed: storage init result is false");
    testFailed = true;
    failReason = 5;
    return;
  }

  if (sdc->getRsaPublicKey() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing public RSA key");
    testFailed = true;
    failReason = 6;
    return;
  } else {
    bool emptyRsa = true;
    auto rsa = sdc->getRsaPublicKey();
    for (int i = 0; i < 512; i++) {
      if (rsa[i] != 0) {
        emptyRsa = false;
      }
    }
    if (emptyRsa) {
      SUPLA_LOG_ERROR("TEST failed: public RSA set, but it is empty");
      testFailed = true;
      failReason = 7;
      return;
    }
  }

  auto srpc = sdc->getSrpcLayer();
  if (srpc == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing srpc layer");
    testFailed = true;
    failReason = 8;
    return;
  }
  if (srpc->getSuplaCACert() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing Supla CA cert");
    testFailed = true;
    failReason = 9;
    return;
  } else if (strlen(srpc->getSuplaCACert()) <= 0) {
    SUPLA_LOG_ERROR("TEST failed: Supla CA cert is empty");
    testFailed = true;
    failReason = 10;
    return;
  }
  if (srpc->getSupla3rdPartyCACert() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing Supla 3rd party CA cert");
    testFailed = true;
    failReason = 11;
    return;
  } else if (strlen(srpc->getSupla3rdPartyCACert()) <= 0) {
    SUPLA_LOG_ERROR("TEST failed: Supla 3rd party CA cert is empty");
    testFailed = true;
    failReason = 12;
    return;
  }
}

void FactoryTest::iterateAlways() {
  if (!testingMachineEnabled) {
    return;
  }

  if (!checkTestStep()) {
    SUPLA_LOG_ERROR("TEST[%d,%d]: check test step failed",
        testStage, testStep);
    testFailed = true;
    // failReason should be set in checkTestStep
    // if it wasn't, fill it with default value
    if (failReason == 0) {
      failReason = 13;
    }
  }

  if (!testFinished) {
    // check timeout
    if (millis() - initTimestamp > timeoutS * 1000) {
      SUPLA_LOG_ERROR("TEST[%d,%d]: timeout %d s elapsed. Failing...",
                      testStage,
                      testStep,
                      timeoutS);
      testFailed = true;
      failReason = 14;
    }
    //
    if (testFailed) {
      SUPLA_LOG_ERROR(
          "TEST[%d,%d]: failed, schedule reset", testStage, testStep);
      sdc->scheduleSoftRestart(2000);
      testFinished = true;
      testingMachineEnabled = false;
    }
  } else if (!testFailed) {
    // test finished successfully
    SUPLA_LOG_INFO(
        "TEST[%d,%d]: testing done - SUCCESS!!!", testStage, testStep);
    sdc->handleAction(0, Supla::RESET_TO_FACTORY_SETTINGS);
    sdc->resetToFactorySettings();
    sdc->scheduleSoftRestart(1000);
    testingMachineEnabled = false;
  }
}

void FactoryTest::handleAction(int event, int action) {
  if (!testingMachineEnabled) {
    return;
  }

  switch (action) {
    case Supla::TestAction_DeviceStatusChange: {
      if (waitForRegisteredAndReady) {
        if (sdc->getCurrentStatus() == STATUS_REGISTERED_AND_READY) {
          waitForRegisteredAndReady = false;
          if (testStage != Supla::TestStage_Init) {
            SUPLA_LOG_ERROR(
                "TEST[%d,%d]: invalid test stage for registered "
                "and ready event",
                testStage,
                testStep);
            testFailed = true;
            break;
          }
          testStage = Supla::TestStage_RegisteredAndReady;
        }
      }
      break;
    }
    case Supla::TestAction_CfgButtonOnPress: {
      if (testStage < Supla::TestStage_RegisteredAndReady) {
        SUPLA_LOG_INFO("TEST[%d,%d]: cfg button press received, but ignored",
                       testStage,
                       testStep);
      } else if (testStage != Supla::TestStage_WaitForCfgButton) {
        SUPLA_LOG_ERROR("TEST[%d,%d]: unexpected cfg button press received",
                        testStage,
                        testStep);
        testFailed = true;
      } else if (testStage == Supla::TestStage_WaitForCfgButton) {
        // success
        testFinished = true;
      }
      break;
    }
    default: {
      SUPLA_LOG_ERROR(
          "TEST[%d,%d]: failed, received unexpected action %d from event %d",
          testStage,
          testStep,
          action,
          event);
      testFailed = true;
      break;
    }
  }
}

void FactoryTest::waitForConfigButtonPress() {
  if (testStage != Supla::TestStage_RegisteredAndReady) {
    SUPLA_LOG_ERROR(
        "TEST[%d,%d]: failed, waitForConfigButtonPress called"
        " in invalid stage",
        testStage,
        testStep);
    testFailed = true;
    return;
  }
  SUPLA_LOG_INFO(
      "TEST[%d,%d]: waiting for config button press", testStage, testStep);
  // led blink
  testStage = Supla::TestStage_WaitForCfgButton;
  sdc->status(STATUS_TEST_WAIT_FOR_CFG_BUTTON, "Wait for config button");
}

bool FactoryTest::checkTestStep() {
  return true;
}

}  // namespace Device
}  // namespace Supla
