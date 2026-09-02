// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_PV_AFORE_H_
#define SRC_SUPLA_PV_AFORE_H_

#include <IPAddress.h>
#include <supla/sensor/one_phase_electricity_meter.h>
#include <supla/network/client.h>

#define LOGIN_AND_PASSOWORD_MAX_LENGTH 100

namespace Supla {
namespace PV {
class Afore : public Supla::Sensor::OnePhaseElectricityMeter {
 public:
  Afore(IPAddress ip, int port, const char *loginAndPassword);
  void readValuesFromDevice();
  void iterateAlways();
  bool iterateConnected();

 protected:
  ::Supla::Client *client = nullptr;
  IPAddress ip;
  int port;
  char loginAndPassword[LOGIN_AND_PASSOWORD_MAX_LENGTH];
  char buf[80];
  unsigned _supla_int64_t totalGeneratedEnergy;
  _supla_int_t currentPower;
  int bytesCounter;
  int retryCounter;
  bool vFound;
  bool varFound;
  bool dataIsReady;
  bool dataFetchInProgress;
  uint32_t connectionTimeoutMs;
};
};  // namespace PV
};  // namespace Supla

#endif  // SRC_SUPLA_PV_AFORE_H_
