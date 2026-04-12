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

/*
 Library tested on:
 Fronius Symo 6.0-3 M with DataManager 2.0 and Smart Meter 63A-3
*/

#ifndef SRC_SUPLA_PV_FRONIUS_H_
#define SRC_SUPLA_PV_FRONIUS_H_

#include <IPAddress.h>
#include <supla/sensor/electricity_meter.h>
#include <supla/network/client.h>

namespace Supla {
namespace PV {
class Fronius : public Supla::Sensor::ElectricityMeter {
 public:
  explicit Fronius(
    IPAddress ip,
    int port = 80,
    int deviceId = 1,
    int deviceType = 0);
  ~Fronius();
  void readValuesFromDevice();
  void iterateAlways();
  bool iterateConnected();

 protected:
  void getSinglePhaseInverterValues(char* varName, char* varValue);
  void getThreePhaseInverterValues(char* varName, char* varValue);
  void getThreePhaseMeterValues(char* varName, char* varValue);
  void setSinglePhaseInverterValues(bool zeroValues);
  void setThreePhaseInverterValues(bool zeroValues);
  void setThreePhaseMeterValues(bool zeroValues);
  void getSinglePhaseInverterURL(char* buf, char* idBuf);
  void getThreePhaseInverterURL(char* buf, char* idBuf);
  void getThreePhaseMeterURL(char* buf, char* idBuf);
  ::Supla::Client *client = nullptr;
  IPAddress ip;
  int port;
  int deviceType;
  char buf[80] = {};
  unsigned _supla_int64_t totalGeneratedEnergy = 0;
  unsigned _supla_int64_t fwdReactEnergy = 0;
  unsigned _supla_int64_t rvrReactEnergy = 0;
  unsigned _supla_int64_t fwdActEnergy = 0;
  unsigned _supla_int64_t rvrActEnergy = 0;
  _supla_int_t currentActivePower[3] = {};
  _supla_int_t currentApparentPower[3] = {};
  _supla_int_t currentReactivePower[3] = {};
  _supla_int_t currentPowerFactor[3] = {};
  unsigned _supla_int16_t currentCurrent[3] = {};
  unsigned _supla_int16_t currentFreq = 0;
  unsigned _supla_int16_t currentVoltage[3] = {};
  int bytesCounter = 0;
  int retryCounter = 0;
  int deviceId;
  bool startCharFound = false;
  bool dataIsReady = false;
  bool dataFetchInProgress = false;
  uint64_t connectionTimeoutMs = 0;
  char variableToFetch[80] = {};
  bool fetch3p = false;
  int invDisabledCounter = 0;
};
};  // namespace PV
};  // namespace Supla

#endif  // SRC_SUPLA_PV_FRONIUS_H_
