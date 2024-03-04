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

#include "condition_getter.h"
#include "element.h"

namespace Supla {

TElectricityMeter_Measurement *ConditionGetter::getMeasurement(
      Supla::Element *element, _supla_int_t *measuredValues) {
  if (!element || !element->getChannel() ||
      !element->getChannel()->getExtValue() ||
      element->getChannel()->getChannelType() !=
      SUPLA_CHANNELTYPE_ELECTRICITY_METER) {
    return nullptr;
  }

  TSuplaChannelExtendedValue *extValue = element->getChannel()->getExtValue();
  if (extValue->type != EV_TYPE_ELECTRICITY_METER_MEASUREMENT_V2) {
    return nullptr;
  }

  TElectricityMeter_ExtendedValue_V2 *emValue =
    reinterpret_cast<TElectricityMeter_ExtendedValue_V2 *>(extValue->value);

  if (emValue->m_count < 1) {
    return nullptr;
  }

  *measuredValues = emValue->measured_values;
  return &(emValue->m[0]);
}

class VoltageGetter : public ConditionGetter {
 public:
  explicit VoltageGetter(int8_t phase) : phase(phase) {}

  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;
    if (phase < 0 || phase >= 3 /* MAX_PHASES */) {
      return 0;
    }

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      if (measuredValues & EM_VAR_VOLTAGE) {
        *isValid = true;
        return measurement->voltage[phase] / 100.0;
      }
    }

    return 0;
  }

 protected:
  int8_t phase = 0;
};

class CurrentGetter : public ConditionGetter {
 public:
  explicit CurrentGetter(int8_t phase) : phase(phase) {
  }

  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;
    if (phase < 0 || phase >= 3 /* MAX_PHASES */) {
      return 0;
    }

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      if (measuredValues & EM_VAR_CURRENT) {
        *isValid = true;
        return measurement->current[phase] / 1000.0;
      }
      if (measuredValues & EM_VAR_CURRENT_OVER_65A) {
        *isValid = true;
        return measurement->current[phase] / 100.0;
      }
    }

    return 0;
  }

 protected:
  int8_t phase = 0;
};

class TotalCurrentGetter : public ConditionGetter {
 public:
  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      double totalCurrent =
        (measurement->current[0] + measurement->current[1] +
         measurement->current[2]);
      if (measuredValues & EM_VAR_CURRENT) {
        *isValid = true;
        return totalCurrent / 1000.0;
      }
      if (measuredValues & EM_VAR_CURRENT_OVER_65A) {
        *isValid = true;
        return totalCurrent / 100.0;
      }
    }

    return 0;
  }
};

class PowerActiveWGetter : public ConditionGetter {
 public:
  explicit PowerActiveWGetter(int8_t phase) : phase(phase) {}

  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;
    if (phase < 0 || phase >= 3 /* MAX_PHASES */) {
      return 0;
    }

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      if (measuredValues & EM_VAR_POWER_ACTIVE) {
        *isValid = true;
        return measurement->power_active[phase] / 100000.0;
      } else if (measuredValues & EM_VAR_POWER_ACTIVE_KW) {
        *isValid = true;
        return measurement->power_active[phase] / 100.0;
      }
    }
    return 0;
  }

 protected:
  int8_t phase = 0;
};

class TotalPowerActiveWGetter : public ConditionGetter {
 public:
  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      double totalPower =
        measurement->power_active[0] + measurement->power_active[1] +
        measurement->power_active[2];
      if (measuredValues & EM_VAR_POWER_ACTIVE) {
        *isValid = true;
        return totalPower / 100000.0;
      } else if (measuredValues & EM_VAR_POWER_ACTIVE_KW) {
        *isValid = true;
        return totalPower / 100.0;
      }
    }

    return 0;
  }
};

class PowerApparentVAGetter : public ConditionGetter {
 public:
  explicit PowerApparentVAGetter(int8_t phase) : phase(phase) {}

  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;
    if (phase < 0 || phase >= 3 /* MAX_PHASES */) {
      return 0;
    }

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      if (measuredValues & EM_VAR_POWER_APPARENT) {
        *isValid = true;
        return measurement->power_apparent[phase] / 100000.0;
      } else if (measuredValues & EM_VAR_POWER_APPARENT_KVA) {
        *isValid = true;
        return measurement->power_apparent[phase] / 100.0;
      }
    }

    return 0;
  }

 protected:
  int8_t phase = 0;
};

class TotalPowerApparentVAGetter : public ConditionGetter {
 public:
  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      double totalPower =
        measurement->power_apparent[0] + measurement->power_apparent[1] +
        measurement->power_apparent[2];
      if (measuredValues & EM_VAR_POWER_APPARENT) {
        *isValid = true;
        return totalPower / 100000.0;
      } else if (measuredValues & EM_VAR_POWER_APPARENT_KVA) {
        *isValid = true;
        return totalPower / 100.0;
      }
    }

    return 0;
  }
};

class PowerReactiveVarGetter : public ConditionGetter {
 public:
  explicit PowerReactiveVarGetter(int8_t phase) : phase(phase) {}

  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;
    if (phase < 0 || phase >= 3 /* MAX_PHASES */) {
      return 0;
    }

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      if (measuredValues & EM_VAR_POWER_REACTIVE) {
        *isValid = true;
        return measurement->power_reactive[phase] / 100000.0;
      } else if (measuredValues & EM_VAR_POWER_REACTIVE_KVAR) {
        *isValid = true;
        return measurement->power_reactive[phase] / 100.0;
      }
    }
    return 0;
  }

 protected:
  int8_t phase = 0;
};

class TotalPowerReactiveVarGetter : public ConditionGetter {
 public:
  double getValue(Supla::Element *element, bool *isValid) override {
    *isValid = false;

    _supla_int_t measuredValues = 0;
    if (auto measurement = getMeasurement(element, &measuredValues)) {
      double totalPower =
        measurement->power_reactive[0] + measurement->power_reactive[1] +
        measurement->power_reactive[2];
      if (measuredValues & EM_VAR_POWER_REACTIVE) {
        *isValid = true;
        return totalPower / 100000.0;
      } else if (measuredValues & EM_VAR_POWER_REACTIVE_KVAR) {
        *isValid = true;
        return totalPower / 100.0;
      }
    }

    return 0;
  }
};

};  // namespace Supla

Supla::ConditionGetter *EmVoltage(int8_t phase) {
  return new Supla::VoltageGetter(phase);
}

Supla::ConditionGetter *EmCurrent(int8_t phase) {
  return new Supla::CurrentGetter(phase);
}

Supla::ConditionGetter *EmTotalCurrent() {
  return new Supla::TotalCurrentGetter();
}

Supla::ConditionGetter *EmPowerActiveW(int8_t phase) {
  return new Supla::PowerActiveWGetter(phase);
}

Supla::ConditionGetter *EmTotalPowerActiveW() {
  return new Supla::TotalPowerActiveWGetter();
}

Supla::ConditionGetter *EmPowerApparentVA(int8_t phase) {
  return new Supla::PowerApparentVAGetter(phase);
}

Supla::ConditionGetter *EmTotalPowerApparentVA() {
  return new Supla::TotalPowerApparentVAGetter();
}

Supla::ConditionGetter *EmPowerReactiveVar(int8_t phase) {
  return new Supla::PowerReactiveVarGetter(phase);
}

Supla::ConditionGetter *EmTotalPowerReactiveVar() {
  return new Supla::TotalPowerReactiveVarGetter();
}
