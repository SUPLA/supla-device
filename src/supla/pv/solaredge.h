// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PV_SOLAREDGE_H_
#define SRC_SUPLA_PV_SOLAREDGE_H_

#ifndef ARDUINO_ARCH_AVR
// Arduino Mega can't establish https connection, so it can't be supported

#include <supla/clock/clock.h>
#include <supla/network/client.h>
#include <supla/sensor/electricity_meter.h>

#define APIKEY_MAX_LENGTH    100
#define PARAMETER_MAX_LENGTH 20

namespace Supla {
namespace PV {
class SolarEdge : public Supla::Sensor::ElectricityMeter {
 public:
  SolarEdge(const char *apiKeyValue,
            const char *siteIdValue,
            const char *inverterSerialNumberValue,
            Supla::Clock *clock);
  ~SolarEdge();
  void readValuesFromDevice();
  void iterateAlways();
  bool iterateConnected();
  Channel *getSecondaryChannel();

 protected:
  ::Supla::Client *pvClient = nullptr;

  char buf[1024];

  double temperature;
  unsigned _supla_int64_t totalGeneratedEnergy;
  unsigned _supla_int_t currentCurrent[3];
  unsigned _supla_int16_t currentVoltage[3];
  unsigned _supla_int16_t currentFreq;
  _supla_int_t currentApparentPower[3];
  _supla_int_t currentActivePower[3];
  _supla_int_t currentReactivePower[3];
  // acCurrent setCurrent
  // acVoltage
  // acFreq
  // apparentPower
  // activePower
  // ReactivePower
  int bytesCounter;
  int retryCounter;
  bool dataIsReady;
  bool dataFetchInProgress;
  bool headerFound;
  uint32_t connectionTimeoutMs;

  char apiKey[APIKEY_MAX_LENGTH] = {};
  char siteId[PARAMETER_MAX_LENGTH] = {};
  char inverterSerialNumber[PARAMETER_MAX_LENGTH] = {};
  Supla::Clock *clock = nullptr;
  Supla::Channel temperatureChannel;
};
};  // namespace PV
};  // namespace Supla

#endif  // ARDUINO_ARCH_AVR
#endif  // SRC_SUPLA_PV_SOLAREDGE_H_
