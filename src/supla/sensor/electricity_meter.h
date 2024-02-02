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

#ifndef SRC_SUPLA_SENSOR_ELECTRICITY_METER_H_
#define SRC_SUPLA_SENSOR_ELECTRICITY_METER_H_

#include <supla-common/srpc.h>
#include <supla/action_handler.h>
#include <supla/element_with_channel_actions.h>

#include "../channel_extended.h"
#include "../local_action.h"

#define MAX_PHASES 3

namespace Supla {
namespace Sensor {

#pragma pack(push, 1)
struct EnergyMeasurmentsStorage {
  _supla_int64_t totalFwdActEnergy = 0;
  _supla_int64_t totalRvrActEnergy = 0;
  _supla_int64_t totalFwdReactEnergy = 0;
  _supla_int64_t totalRvrReactEnergy = 0;
};
#pragma pack(pop)

#define EM_VAR_ALL_ENERGY_REGISTERS                                  \
  (EM_VAR_FORWARD_ACTIVE_ENERGY | EM_VAR_REVERSE_ACTIVE_ENERGY |     \
  EM_VAR_FORWARD_REACTIVE_ENERGY | EM_VAR_REVERSE_REACTIVE_ENERGY |  \
  EM_VAR_FORWARD_ACTIVE_ENERGY_BALANCED |                            \
  EM_VAR_REVERSE_ACTIVE_ENERGY_BALANCED)

class ElectricityMeter : public ElementWithChannelActions,
                         public ActionHandler {
 public:
  ElectricityMeter();

  virtual void updateChannelValues();

  // energy in 0.00001 kWh
  void setFwdActEnergy(int phase, unsigned _supla_int64_t energy);

  // energy in 0.00001 kWh
  void setRvrActEnergy(int phase, unsigned _supla_int64_t energy);

  // energy in 0.00001 kWh
  void setFwdReactEnergy(int phase, unsigned _supla_int64_t energy);

  // energy in 0.00001 kWh
  void setRvrReactEnergy(int phase, unsigned _supla_int64_t energy);

  // voltage in 0.01 V
  void setVoltage(int phase, unsigned _supla_int16_t voltage);

  // current in 0.001 A
  void setCurrent(int phase, unsigned _supla_int_t current);

  // Frequency in 0.01 Hz
  void setFreq(unsigned _supla_int16_t freq);

  // power in 0.00001 W
  void setPowerActive(int phase, _supla_int_t power);

  // power in 0.00001 var
  void setPowerReactive(int phase, _supla_int_t power);

  // power in 0.00001 VA
  void setPowerApparent(int phase, _supla_int_t power);

  // power in 0.001
  void setPowerFactor(int phase, _supla_int_t powerFactor);

  // phase angle in 0.1 degree
  void setPhaseAngle(int phase, _supla_int_t phaseAngle);

  // energy 1 == 0.00001 kWh
  unsigned _supla_int64_t getFwdActEnergy(int phase);

  // energy 1 == 0.00001 kWh
  unsigned _supla_int64_t getRvrActEnergy(int phase);

  // energy 1 == 0.00001 kWh
  unsigned _supla_int64_t getFwdReactEnergy(int phase);

  // energy 1 == 0.00001 kWh
  unsigned _supla_int64_t getRvrReactEnergy(int phase);

  // voltage 1 == 0.01 V
  unsigned _supla_int16_t getVoltage(int phase);

  // current 1 == 0.001 A
  unsigned _supla_int_t getCurrent(int phase);

  // Frequency 1 == 0.01 Hz
  unsigned _supla_int16_t getFreq();

  // power 1 == 0.00001 W
  _supla_int_t getPowerActive(int phase);

  // power 1 == 0.00001 var
  _supla_int_t getPowerReactive(int phase);

  // power 1 == 0.00001 VA
  _supla_int_t getPowerApparent(int phase);

  // power 1 == 0.001
  _supla_int_t getPowerFactor(int phase);

  // phase angle 1 == 0.1 degree
  _supla_int_t getPhaseAngle(int phase);

  // energy 1 == 0.00001 kWh
  static unsigned _supla_int64_t
    getFwdActEnergy(const TElectricityMeter_ExtendedValue_V2 &emValue,
        int phase);

  // energy 1 == 0.00001 kWh
  static unsigned _supla_int64_t
    getTotalFwdActEnergy(const TElectricityMeter_ExtendedValue_V2 &emValue);

  // energy 1 == 0.00001 kWh
  static unsigned _supla_int64_t
    getRvrActEnergy(const TElectricityMeter_ExtendedValue_V2 &emValue,
        int phase);

  // energy 1 == 0.00001 kWh
  static unsigned _supla_int64_t
    getTotalRvrActEnergy(const TElectricityMeter_ExtendedValue_V2 &emValue);

  // energy 1 == 0.00001 kWh
  static unsigned _supla_int64_t
    getFwdReactEnergy(const TElectricityMeter_ExtendedValue_V2 &emValue,
        int phase);

  // energy 1 == 0.00001 kWh
  static unsigned _supla_int64_t
    getRvrReactEnergy(const TElectricityMeter_ExtendedValue_V2 &emValue,
        int phase);

  // voltage 1 == 0.01 V
  static unsigned _supla_int16_t
    getVoltage(const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  // current 1 == 0.001 A
  static unsigned _supla_int_t
    getCurrent(const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  // Frequency 1 == 0.01 Hz
  static unsigned _supla_int16_t
    getFreq(const TElectricityMeter_ExtendedValue_V2 &emValue);

  // power 1 == 0.00001 W
  static _supla_int_t getPowerActive(
      const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  // power 1 == 0.00001 var
  static _supla_int_t getPowerReactive(
      const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  // power 1 == 0.00001 VA
  static _supla_int_t getPowerApparent(
      const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  // power 1 == 0.001
  static _supla_int_t getPowerFactor(
      const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  // phase angle 1 == 0.1 degree
  static _supla_int_t getPhaseAngle(
      const TElectricityMeter_ExtendedValue_V2 &emValue, int phase);

  static bool isFwdActEnergyUsed(
    const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isRvrActEnergyUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isFwdReactEnergyUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isRvrReactEnergyUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isVoltageUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isCurrentUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isFreqUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isPowerActiveUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isPowerReactiveUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isPowerApparentUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isPowerFactorUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  static bool isPhaseAngleUsed(
      const TElectricityMeter_ExtendedValue_V2 &emValue);

  void resetReadParameters();

  // Please implement this class for reading value from elecricity meter device.
  // It will be called every 5 s. Use set methods defined above in order to
  // set values on channel. Don't use any other method to modify channel values.
  virtual void readValuesFromDevice();

  // Put here initialization code for electricity meter device.
  // It will be called within SuplaDevce.begin method.
  // It should also read first data set, so at the end it should call those two
  // methods:
  // readValuesFromDevice();
  // updateChannelValues();
  void onInit() override;

  void iterateAlways() override;

  // Implement this method to reset stored energy value (i.e. to set energy
  // counter back to 0 kWh
  virtual void resetStorage();

  // default calcfg implementation allows to resetStorage remotely.
  // If you override it please remember to implement this functionality
  // or call this method from base classs.
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;

  void handleAction(int event, int action) override;

  void setRefreshRate(unsigned int sec);

  Channel *getChannel() override;

 protected:
  TElectricityMeter_ExtendedValue_V2 emValue = {};
  ChannelExtended extChannel;
  unsigned _supla_int_t rawCurrent[MAX_PHASES] = {};
  uint32_t lastReadTime = 0;
  uint8_t refreshRateSec = 5;
  bool valueChanged = false;
  bool currentMeasurementAvailable = false;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_ELECTRICITY_METER_H_
