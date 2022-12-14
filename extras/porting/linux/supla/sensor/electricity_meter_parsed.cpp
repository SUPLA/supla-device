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

#include "electricity_meter_parsed.h"
#include <supla/log_wrapper.h>

Supla::Sensor::ElectricityMeterParsed::ElectricityMeterParsed(
    Supla::Parser::Parser *parser) :
  Supla::Sensor::SensorParsed<Supla::Sensor::ElectricityMeter>(parser) {}

void Supla::Sensor::ElectricityMeterParsed::onInit() {
  readValuesFromDevice();
  updateChannelValues();
}

void Supla::Sensor::ElectricityMeterParsed::readValuesFromDevice() {
  if (refreshParserSource()) {
    if (isParameterConfigured(Supla::Parser::FwdActEnergy1)) {
      setFwdActEnergy(0,
          getParameterValue(Supla::Parser::FwdActEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdActEnergy2)) {
      setFwdActEnergy(1,
          getParameterValue(Supla::Parser::FwdActEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdActEnergy3)) {
      setFwdActEnergy(2,
          getParameterValue(Supla::Parser::FwdActEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::RvrActEnergy1)) {
      setRvrActEnergy(0,
          getParameterValue(Supla::Parser::RvrActEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrActEnergy2)) {
      setRvrActEnergy(1,
          getParameterValue(Supla::Parser::RvrActEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrActEnergy3)) {
      setRvrActEnergy(2,
          getParameterValue(Supla::Parser::RvrActEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::FwdReactEnergy1)) {
      setFwdReactEnergy(
          0, getParameterValue(Supla::Parser::FwdReactEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdReactEnergy2)) {
      setFwdReactEnergy(
          1, getParameterValue(Supla::Parser::FwdReactEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::FwdReactEnergy3)) {
      setFwdReactEnergy(
          2, getParameterValue(Supla::Parser::FwdReactEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::RvrReactEnergy1)) {
      setRvrReactEnergy(
          0, getParameterValue(Supla::Parser::RvrReactEnergy1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrReactEnergy2)) {
      setRvrReactEnergy(
          1, getParameterValue(Supla::Parser::RvrReactEnergy2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::RvrReactEnergy3)) {
      setRvrReactEnergy(
          2, getParameterValue(Supla::Parser::RvrReactEnergy3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::Current1)) {
      setCurrent(0, getParameterValue(Supla::Parser::Current1) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::Current2)) {
      setCurrent(1, getParameterValue(Supla::Parser::Current2) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::Current3)) {
      setCurrent(2, getParameterValue(Supla::Parser::Current3) * 1000);
    }

    // Supla shows signed power (negative value means power returned to grid)
    if (isParameterConfigured(Supla::Parser::PowerActive1)) {
      if (isParameterConfigured(Supla::Parser::RvrPowerActive1)) {
        setPowerActive(0,
            (getParameterValue(Supla::Parser::PowerActive1) -
             getParameterValue(Supla::Parser::RvrPowerActive1)) *
            100000);
      } else {
        setPowerActive(0,
            getParameterValue(Supla::Parser::PowerActive1) * 100000);
      }
    } else if (isParameterConfigured(Supla::Parser::RvrPowerActive1)) {
      setPowerActive(
          0, -getParameterValue(Supla::Parser::RvrPowerActive1) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerActive2)) {
      if (isParameterConfigured(Supla::Parser::RvrPowerActive2)) {
        setPowerActive(1,
            (getParameterValue(Supla::Parser::PowerActive2) -
             getParameterValue(Supla::Parser::RvrPowerActive2)) *
            100000);
      } else {
        setPowerActive(1,
            getParameterValue(Supla::Parser::PowerActive2) * 100000);
      }
    } else if (isParameterConfigured(Supla::Parser::RvrPowerActive2)) {
      setPowerActive(
          1, -getParameterValue(Supla::Parser::RvrPowerActive2) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerActive3)) {
      if (isParameterConfigured(Supla::Parser::RvrPowerActive3)) {
        setPowerActive(2,
            (getParameterValue(Supla::Parser::PowerActive3) -
             getParameterValue(Supla::Parser::RvrPowerActive3)) *
            100000);
      } else {
        setPowerActive(2,
            getParameterValue(Supla::Parser::PowerActive3) * 100000);
      }
    } else if (isParameterConfigured(Supla::Parser::RvrPowerActive3)) {
      setPowerActive(
          2, -getParameterValue(Supla::Parser::RvrPowerActive3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerReactive1)) {
      setPowerReactive(
          0, getParameterValue(Supla::Parser::PowerReactive1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerReactive2)) {
      setPowerReactive(
          1, getParameterValue(Supla::Parser::PowerReactive2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerReactive3)) {
      setPowerReactive(
          2, getParameterValue(Supla::Parser::PowerReactive3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerApparent1)) {
      setPowerApparent(
          0, getParameterValue(Supla::Parser::PowerApparent1) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerApparent2)) {
      setPowerApparent(
          1, getParameterValue(Supla::Parser::PowerApparent2) * 100000);
    }
    if (isParameterConfigured(Supla::Parser::PowerApparent3)) {
      setPowerApparent(
          2, getParameterValue(Supla::Parser::PowerApparent3) * 100000);
    }

    if (isParameterConfigured(Supla::Parser::PowerFactor1)) {
      setPowerFactor(0, getParameterValue(Supla::Parser::PowerFactor1) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::PowerFactor2)) {
      setPowerFactor(1, getParameterValue(Supla::Parser::PowerFactor2) * 1000);
    }
    if (isParameterConfigured(Supla::Parser::PowerFactor3)) {
      setPowerFactor(2, getParameterValue(Supla::Parser::PowerFactor3) * 1000);
    }

    if (isParameterConfigured(Supla::Parser::PhaseAngle1)) {
      setPhaseAngle(0, getParameterValue(Supla::Parser::PhaseAngle1) * 10);
    }
    if (isParameterConfigured(Supla::Parser::PhaseAngle2)) {
      setPhaseAngle(1, getParameterValue(Supla::Parser::PhaseAngle2) * 10);
    }
    if (isParameterConfigured(Supla::Parser::PhaseAngle3)) {
      setPhaseAngle(2, getParameterValue(Supla::Parser::PhaseAngle3) * 10);
    }

    if (isParameterConfigured(Supla::Parser::Voltage1)) {
      setVoltage(0, getParameterValue(Supla::Parser::Voltage1) * 100);
    }
    if (isParameterConfigured(Supla::Parser::Voltage2)) {
      setVoltage(1, getParameterValue(Supla::Parser::Voltage2) * 100);
    }
    if (isParameterConfigured(Supla::Parser::Voltage3)) {
      setVoltage(2, getParameterValue(Supla::Parser::Voltage3) * 100);
    }

    if (isParameterConfigured(Supla::Parser::Frequency)) {
      setFreq(getParameterValue(Supla::Parser::Frequency) * 100);
    }
    isDataErrorLogged = false;

  } else {
    if (!isDataErrorLogged) {
      isDataErrorLogged = true;
      SUPLA_LOG_WARNING("ElectricityMeterParsed: data source error");
    }
    resetReadParameters();
  }
}
