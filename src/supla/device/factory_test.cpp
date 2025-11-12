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
#include <string.h>
#include <supla/actions.h>
#include <supla/device/register_device.h>
#include <supla/protocol/supla_srpc.h>
#include <supla/storage/storage.h>
#include <supla/storage/config.h>
#include <supla/network/web_server.h>
#include <supla/clock/clock.h>

#include "factory_test.h"

#ifdef ESP8266
#define FLASH_STRLEN(p) strlen_P(p)
#else
#define FLASH_STRLEN(p) strlen(p)
#endif


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
  bool selfTestMode =
      (sdc->getDeviceMode() != Supla::DeviceMode::DEVICE_MODE_TEST);

  if (!selfTestMode) {
    SUPLA_LOG_INFO("TEST[%d,%d] started", testStage, testStep);
    sdc->testStepStatusLed(2);
  }

  if (testStage != Supla::TestStage_None || testStep != 0) {
    SUPLA_LOG_ERROR(
        "TEST[%d,%d] failed: invalid step/stage sequence", testStage, testStep);
    testFailed = true;
    return;
  }

  testStage = Supla::TestStage_Init;

  if (!selfTestMode) {
    if (Supla::RegisterDevice::getManufacturerId() != getManufacturerId()) {
      SUPLA_LOG_ERROR("TEST failed: invalid ManufacturerID");
      testFailed = true;
      failReason = 1;
      return;
    }
  } else {
    SUPLA_LOG_INFO("TEST: ManufacturerID is %d",
                   Supla::RegisterDevice::getManufacturerId());
  }

  if (Supla::RegisterDevice::getProductId() == 0) {
    SUPLA_LOG_ERROR("TEST failed: ProductID is empty");
    testFailed = true;
    failReason = 2;
    if (!selfTestMode) {
      return;
    }
  }

  if (Supla::RegisterDevice::isNameEmpty()) {
    SUPLA_LOG_ERROR("TEST failed: device name is empty");
    testFailed = true;
    failReason = 3;
    if (!selfTestMode) {
      return;
    }
  }

  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing Config instance");
    testFailed = true;
    failReason = 4;
    if (!selfTestMode) {
      return;
    }
  }

  if (!sdc->getStorageInitResult()) {
    SUPLA_LOG_ERROR("TEST failed: storage init result is false");
    testFailed = true;
    failReason = 5;
    if (!selfTestMode) {
      return;
    }
  }

  if (sdc->getRsaPublicKey() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing public RSA key");
    testFailed = true;
    failReason = 6;
    if (!selfTestMode) {
      return;
    }
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
      if (!selfTestMode) {
        return;
      }
    }
  }

  auto srpc = sdc->getSrpcLayer();
  if (srpc == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing srpc layer");
    testFailed = true;
    failReason = 8;
    if (!selfTestMode) {
      return;
    }
  }
  if (srpc->getSuplaCACert() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing Supla CA cert");
    testFailed = true;
    failReason = 9;
    if (!selfTestMode) {
      return;
    }
  } else if (FLASH_STRLEN(srpc->getSuplaCACert()) <= 0) {
    SUPLA_LOG_ERROR("TEST failed: Supla CA cert is empty");
    testFailed = true;
    failReason = 10;
    if (!selfTestMode) {
      return;
    }
  }
  if (srpc->getSupla3rdPartyCACert() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing Supla 3rd party CA cert");
    testFailed = true;
    failReason = 11;
    if (!selfTestMode) {
      return;
    }
  } else if (FLASH_STRLEN(srpc->getSupla3rdPartyCACert()) <= 0) {
    SUPLA_LOG_ERROR("TEST failed: Supla 3rd party CA cert is empty");
    testFailed = true;
    failReason = 12;
    if (!selfTestMode) {
      return;
    }
  }

  if (checkAutomaticFirmwareUpdate &&
      !sdc->isAutomaticFirmwareUpdateEnabled()) {
    SUPLA_LOG_ERROR("TEST failed: automatic firmware update is disabled");
    testFailed = true;
    failReason = 15;
    if (!selfTestMode) {
      return;
    }
  }

  if (cfg && !cfg->isEncryptionEnabled()) {
    SUPLA_LOG_ERROR("TEST failed: config encryption is disabled");
#ifndef SUPLA_DEBUG
    testFailed = true;
    failReason = 16;
    if (!selfTestMode) {
      return;
    }
#endif
  }

  auto webServer = Supla::WebServer::Instance();
  if (webServer == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing web server");
    testFailed = true;
    failReason = 17;
    if (!selfTestMode) {
      return;
    }
  }
  if (webServer && !webServer->verifyCertificatesFormat()) {
    SUPLA_LOG_ERROR("TEST failed: invalid certificates format");
    testFailed = true;
    failReason = 18;
    if (!selfTestMode) {
      return;
    }
  }

  if (Supla::Clock::GetInstance() == nullptr) {
    SUPLA_LOG_ERROR("TEST failed: missing clock instance");
    testFailed = true;
    failReason = 19;
    if (!selfTestMode) {
      return;
    }
  }

  if (!sdc->isSecurityLogEnabled()) {
    SUPLA_LOG_ERROR("TEST failed: security log is disabled");
    testFailed = true;
    failReason = 20;
    if (!selfTestMode) {
      return;
    }
  }

  if (sdc->getInitialMode() == Supla::InitialMode::StartInCfgMode) {
    SUPLA_LOG_ERROR("TEST failed: initial mode is set to config mode");
    testFailed = true;
    failReason = 21;
    if (!selfTestMode) {
      return;
    }
  }
}

void FactoryTest::iterateAlways() {
  if (!testingMachineEnabled) {
    return;
  }
  if (sdc->getCurrentStatus() == STATUS_CONFIG_MODE) {
    // ignore factory test in config mode
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
          sdc->testStepStatusLed(4);
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
  sdc->status(STATUS_TEST_WAIT_FOR_CFG_BUTTON, F("Wait for config button"));
}

bool FactoryTest::checkTestStep() {
  return true;
}

void FactoryTest::setTestFailed(int reason) {
  testFailed = true;
  failReason = reason;
}

void FactoryTest::setTestFinished() {
  testFinished = true;
}

Supla::TestStage FactoryTest::getTestStage() const {
  return testStage;
}

void FactoryTest::dontCheckAutomaticFirmwareUpdate() {
  checkAutomaticFirmwareUpdate = false;
}

}  // namespace Device
}  // namespace Supla
