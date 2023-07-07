/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

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

#ifndef SRC_SUPLA_PV_FRONIUS_H_
#define SRC_SUPLA_PV_FRONIUS_H_

#include <IPAddress.h>
#include <supla/sensor/one_phase_electricity_meter.h>
#include <supla/network/client.h>

namespace Supla {
namespace PV {
class Fronius : public Supla::Sensor::OnePhaseElectricityMeter {
 public:
  explicit Fronius(IPAddress ip, int port = 80, int deviceId = 1);
  ~Fronius();
  void readValuesFromDevice();
  void iterateAlways();
  bool iterateConnected();

 protected:
  ::Supla::Client *client = nullptr;
  IPAddress ip;
  int port;
  char buf[80];
  unsigned _supla_int64_t totalGeneratedEnergy;
  _supla_int_t currentPower;
  unsigned _supla_int16_t currentCurrent;
  unsigned _supla_int16_t currentFreq;
  unsigned _supla_int16_t currentVoltage;
  int bytesCounter;
  int retryCounter;
  int valueToFetch;
  int deviceId;
  bool startCharFound;
  bool dataIsReady;
  bool dataFetchInProgress;
  uint32_t connectionTimeoutMs;
};
};  // namespace PV
};  // namespace Supla

#endif  // SRC_SUPLA_PV_FRONIUS_H_
