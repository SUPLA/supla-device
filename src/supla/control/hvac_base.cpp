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

#include "hvac_base.h"

#include <stdint.h>
#include <string.h>
#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/storage.h>
#include <supla/time.h>
#include <supla/clock/clock.h>
#include <supla/actions.h>
#include <supla/sensor/thermometer.h>
#include <supla/events.h>
#include <supla/channels/channel.h>
#include <supla/storage/config_tags.h>

#include "output_interface.h"
#include "relay_hvac_aggregator.h"

#define SUPLA_HVAC_DEFAULT_TEMP_HEAT          2100  // 21.00 C
#define SUPLA_HVAC_DEFAULT_TEMP_COOL          2500  // 25.00 C

#define USE_MAIN_WEEKLYSCHEDULE (false)
#define USE_ALT_WEEKLYSCHEDULE (true)

using Supla::Control::HvacBase;

HvacBase::HvacBase(Supla::Control::OutputInterface *primaryOutput,
                   Supla::Control::OutputInterface *secondaryOutput) {
  channel.setType(SUPLA_CHANNELTYPE_HVAC);
  channel.setFlag(SUPLA_CHANNEL_FLAG_WEEKLY_SCHEDULE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  channel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  addPrimaryOutput(primaryOutput);
  addSecondaryOutput(secondaryOutput);

  setTemperatureHisteresisMin(20);  // 0.2 degree
  setTemperatureHisteresisMax(1000);  // 10 degree
  setTemperatureHeatCoolOffsetMin(200);   // 2 degrees
  setTemperatureHeatCoolOffsetMax(1000);  // 10 degrees
  setTemperatureAuxMin(500);  // 5 degrees
  setTemperatureAuxMax(7500);  // 75 degrees
  addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
  addAvailableAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);

  // by default binary sensor and aux thermometers are off (set to HVAC
  // channel number)
  defaultBinarySensor = getChannelNumber();
  defaultAuxThermometer = getChannelNumber();

  // default function is set in onInit based on supported modes or loaded from
  // config
}

HvacBase::~HvacBase() {
  Supla::Control::RelayHvacAggregator::UnregisterHvac(this);
}

void HvacBase::handleAction(int event, int action) {
  (void)(event);
  switch (action) {
    case Supla::TURN_ON: {
      setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
      break;
    }
    case Supla::TURN_OFF: {
      setTargetMode(SUPLA_HVAC_MODE_OFF, false);
      break;
    }
    case Supla::TOGGLE: {
      if (channel.getHvacMode() == SUPLA_HVAC_MODE_OFF) {
        setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
      } else {
        setTargetMode(SUPLA_HVAC_MODE_OFF, false);
      }
      break;
    }
    case Supla::INCREASE_TEMPERATURE: {
      changeTemperatureSetpointsBy(buttonTemperatureStep,
                                   buttonTemperatureStep);
      break;
    }
    case Supla::DECREASE_TEMPERATURE: {
      changeTemperatureSetpointsBy(-buttonTemperatureStep,
                                   -buttonTemperatureStep);
      break;
    }
    case Supla::INCREASE_HEATING_TEMPERATURE: {
      changeTemperatureSetpointsBy(buttonTemperatureStep, 0);
      break;
    }
    case Supla::DECREASE_HEATING_TEMPERATURE: {
      changeTemperatureSetpointsBy(-buttonTemperatureStep, 0);
      break;
    }
    case Supla::INCREASE_COOLING_TEMPERATURE: {
      changeTemperatureSetpointsBy(0, buttonTemperatureStep);
      break;
    }
    case Supla::DECREASE_COOLING_TEMPERATURE: {
      changeTemperatureSetpointsBy(0, -buttonTemperatureStep);
      break;
    }
    case Supla::SWITCH_TO_MANUAL_MODE: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL, 0);
      break;
    }
    case Supla::SWITCH_TO_WEEKLY_SCHEDULE_MODE: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE, 0);
      break;
    }
    case Supla::SWITCH_TO_MANUAL_MODE_HEAT: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_HEAT, 0);
      break;
    }
    case Supla::SWITCH_TO_MANUAL_MODE_COOL: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_COOL, 0);
      break;
    }
    case Supla::SWITCH_TO_MANUAL_MODE_HEAT_COOL: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_HEAT_COOL, 0);
      break;
    }
    case Supla::TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES: {
      auto mode = getMode();
      bool isWeeklyScheduleMode = isWeeklyScheduleEnabled();
      if (isWeeklyScheduleMode) {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL, 0);
      } else if (mode == SUPLA_HVAC_MODE_OFF) {
        turnOn();
      } else {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE, 0);
      }
      break;
    }
    case Supla::TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES: {
      auto mode = getMode();
      bool isWeeklyScheduleMode = isWeeklyScheduleEnabled();
      if (isWeeklyScheduleMode) {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_OFF, 0);
      } else if (mode == SUPLA_HVAC_MODE_OFF) {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL, 0);
      } else {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE, 0);
      }
      break;
    }
  }
}

bool HvacBase::iterateConnected() {
  if (timerUpdateTimestamp != countdownTimerEnds && Supla::Clock::IsReady()) {
    timerUpdateTimestamp = countdownTimerEnds;
    updateTimerValue();
  }
  auto result = Element::iterateConnected();

  if (!result) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: send: IsOn %d, Mode %s, tHeat %d, tCool %d, flags 0x%x",
        getChannelNumber(),
        channel.getHvacIsOn(),
        channel.getHvacModeCstr(),
        channel.getHvacSetpointTemperatureHeat(),
        channel.getHvacSetpointTemperatureCool(),
        channel.getHvacFlags());
    return result;
  }

  if (configFinishedReceived && serverChannelFunctionValid) {
    if (channelConfigChangedOffline == 1) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        config.ParameterFlags = parameterFlags;
        if (proto->setChannelConfig(getChannelNumber(),
                                    channel.getDefaultFunction(),
                                    reinterpret_cast<void *>(&config),
                                    sizeof(TChannelConfig_HVAC),
                                    SUPLA_CONFIG_TYPE_DEFAULT)) {
          SUPLA_LOG_INFO("HVAC[%d]: channel config send",
                         getChannelNumber());
          channelConfigChangedOffline = 2;
        }
      }
    }
    if (weeklyScheduleChangedOffline == 1) {
      for (auto proto = Supla::Protocol::ProtocolLayer::first();
           proto != nullptr;
           proto = proto->next()) {
        if (proto->setChannelConfig(getChannelNumber(),
                                    channel.getDefaultFunction(),
                                    reinterpret_cast<void *>(&weeklySchedule),
                                    sizeof(weeklySchedule),
                                    SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE)) {
          SUPLA_LOG_INFO("HVAC[%d]: weekly schedule send",
                         getChannelNumber());
          weeklyScheduleChangedOffline = 2;
          if (channel.getDefaultFunction() ==
              SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
            if (proto->setChannelConfig(
                    getChannelNumber(),
                    channel.getDefaultFunction(),
                    reinterpret_cast<void *>(&altWeeklySchedule),
                    sizeof(altWeeklySchedule),
                    SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE)) {
              SUPLA_LOG_INFO("HVAC[%d]: alt weekly schedule send",
                             getChannelNumber());
            }
          }
        }
      }
    }
  }

  return result;
}

void HvacBase::onLoadConfig(SuplaDeviceClass *sdc) {
  (void)(sdc);
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};

    // Read last set channel function
    loadFunctionFromConfig();
    initDefaultConfig();

    // Generic HVAC configuration
    generateKey(key, Supla::ConfigTag::HvacCfgTag);
    TChannelConfig_HVAC storedConfig = {};
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&storedConfig),
                     sizeof(TChannelConfig_HVAC))) {
      SUPLA_LOG_DEBUG("HVAC[%d]: config in storage:", getChannelNumber());
      debugPrintConfigStruct(&storedConfig, getChannelNumber());
      if (isConfigValid(&storedConfig)) {
        fixReadonlyParameters(&storedConfig);
        applyAdditionalValidation(&storedConfig);
        applyConfigWithoutValidation(&storedConfig);
        fixTemperatureSetpoints();
        SUPLA_LOG_INFO("HVAC[%d]: config loaded successfully",
                       getChannelNumber());
      } else {
        SUPLA_LOG_WARNING(
            "HVAC[%d]: config invalid in storage. Using SW defaults",
            getChannelNumber());
      }
    } else {
      SUPLA_LOG_INFO("HVAC[%d]: config missing. Using SW defaults",
                     getChannelNumber());
    }

    // Weekly schedule configuration
    generateKey(key, Supla::ConfigTag::HvacWeeklyCfgTag);
    isWeeklyScheduleConfigured = false;
    if (cfg->getBlob(key,
                     reinterpret_cast<char *>(&weeklySchedule),
                     sizeof(TChannelConfig_WeeklySchedule))) {
      if (!isWeeklyScheduleValid(&weeklySchedule)) {
        SUPLA_LOG_WARNING(
            "HVAC[%d]: weekly schedule invalid in storage. Using SW "
            "defaults", getChannelNumber());
      } else {
        SUPLA_LOG_INFO("HVAC[%d]: weekly schedule loaded successfully",
                       getChannelNumber());
        isWeeklyScheduleConfigured = true;
      }
    } else {
      SUPLA_LOG_INFO("HVAC[%d]: weekly schedule missing. Using SW defaults",
                     getChannelNumber());
    }

    // Alt weekly schedule (only for HVAC_THERMOSTAT function)
    if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
      generateKey(key, Supla::ConfigTag::HvacAltWeeklyCfgTag);
      if (cfg->getBlob(key,
            reinterpret_cast<char *>(&altWeeklySchedule),
            sizeof(TChannelConfig_WeeklySchedule))) {
        if (!isWeeklyScheduleValid(&altWeeklySchedule, true)) {
          SUPLA_LOG_WARNING(
              "HVAC[%d]: alt weekly schedule invalid in storage. Using SW "
              "defaults", getChannelNumber());
        } else {
          SUPLA_LOG_INFO("HVAC[%d]: alt weekly schedule loaded successfully",
                         getChannelNumber());
          isWeeklyScheduleConfigured = true;
        }
      } else {
        SUPLA_LOG_INFO(
            "HVAC[%d]: alt weekly schedule missing. Using SW defaults",
            getChannelNumber());
      }
    }

    // load config changed offline flags
    if (cfg->isChannelConfigChangeFlagSet(getChannelNumber())) {
      SUPLA_LOG_INFO("HVAC[%d]: config changed offline flag is set",
                     getChannelNumber());
      channelConfigChangedOffline = 1;
    } else {
      channelConfigChangedOffline = 0;
    }

    uint8_t flag = 0;
    generateKey(key, "weekly_chng");
    cfg->getUInt8(key, &flag);
    SUPLA_LOG_INFO("HVAC[%d]: weekly schedule config changed offline flag %d",
                   getChannelNumber(),
                   flag);
    if (flag) {
      weeklyScheduleChangedOffline = 1;
    } else {
      weeklyScheduleChangedOffline = 0;
    }

  } else {
    SUPLA_LOG_ERROR("HVAC[%d]: can't work without config storage",
                    getChannelNumber());
  }
}

void HvacBase::onLoadState() {
  if (getChannelNumber() >= 0) {
    auto hvacValue = channel.getValueHvac();
    THVACValue hvacValueState = {};
    Supla::Storage::ReadState(
        reinterpret_cast<unsigned char *>(&hvacValueState), sizeof(THVACValue));
    if (hvacValue) {
      memcpy(hvacValue, &hvacValueState, sizeof(THVACValue));
    }
    if (!Supla::Channel::isHvacValueValid(hvacValue)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid HVAC value in state",
                        getChannelNumber());
    }

    memset(&lastWorkingMode, 0, sizeof(lastWorkingMode));
    Supla::Storage::ReadState(
        reinterpret_cast<unsigned char *>(&lastWorkingMode),
        sizeof(lastWorkingMode));
    if (!Supla::Channel::isHvacValueValid(&lastWorkingMode)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid lastWorkingMode HVAC value in state",
                        getChannelNumber());
    }

    countdownTimerEnds = 1;
    Supla::Storage::ReadState(
        reinterpret_cast<unsigned char *>(&countdownTimerEnds),
        sizeof(countdownTimerEnds));
    if (countdownTimerEnds <= 0) {
      countdownTimerEnds = 1;
    }

    lastManualSetpointHeat = INT16_MIN;
    Supla::Storage::ReadState(
        reinterpret_cast<unsigned char *>(&lastManualSetpointHeat),
        sizeof(lastManualSetpointHeat));

    lastManualSetpointCool = INT16_MIN;
    Supla::Storage::ReadState(
        reinterpret_cast<unsigned char *>(&lastManualSetpointCool),
        sizeof(lastManualSetpointCool));

    lastManualMode = 0;
    Supla::Storage::ReadState(
        reinterpret_cast<unsigned char *>(&lastManualMode),
        sizeof(lastManualMode));
    if (lastManualMode > SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: invalid lastManualMode HVAC value in state: %d",
          getChannelNumber(),
          lastManualMode);
    }

    SUPLA_LOG_DEBUG(
        "HVAC[%d] onLoadState. hvacValue: IsOn: %d, Mode: %d, "
        "SetpointTemperatureCool: %d, SetpointTemperatureHeat: %d, "
        "Flags: %d",
        getChannelNumber(),
        hvacValue->IsOn,
        hvacValue->Mode,
        hvacValue->SetpointTemperatureCool,
        hvacValue->SetpointTemperatureHeat,
        hvacValue->Flags);
    SUPLA_LOG_DEBUG(
        "HVAC[%d] onLoadState. lastWorkingMode: IsOn: %d, Mode: %d, "
        "SetpointTemperatureCool: %d, SetpointTemperatureHeat: %d, "
        "Flags: %d",
        getChannelNumber(),
        lastWorkingMode.IsOn,
        lastWorkingMode.Mode,
        lastWorkingMode.SetpointTemperatureCool,
        lastWorkingMode.SetpointTemperatureHeat,
        lastWorkingMode.Flags);
    SUPLA_LOG_DEBUG(
        "HVAC[%d] onLoadState. countdownTimerEnds: %lld, "
        "lastManualSetpointCool: %d, lastManualSetpointHeat: %d, "
        "lastManualMode: %d",
        getChannelNumber(),
        countdownTimerEnds,
        lastManualSetpointCool,
        lastManualSetpointHeat,
        lastManualMode);
  }
}

void HvacBase::onSaveState() {
  if (getChannelNumber() >= 0) {
    auto hvacValue = channel.getValueHvac();
    THVACValue hvacValueState = {};
    if (hvacValue) {
      memcpy(&hvacValueState, hvacValue, sizeof(THVACValue));
    }
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(&hvacValueState),
        sizeof(THVACValue));
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(&lastWorkingMode),
        sizeof(lastWorkingMode));
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(&countdownTimerEnds),
        sizeof(countdownTimerEnds));
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(&lastManualSetpointHeat),
        sizeof(lastManualSetpointHeat));
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(&lastManualSetpointCool),
        sizeof(lastManualSetpointCool));
    Supla::Storage::WriteState(
        reinterpret_cast<const unsigned char *>(&lastManualMode),
        sizeof(lastManualMode));
  }
}

void HvacBase::onInit() {
  // init default channel function when it wasn't read from config or
  // set by user
  if (channel.getDefaultFunction() == 0) {
    // set default to heat_cool when both heat and cool are supported
    if (isHeatCoolSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL);
    } else if (isHeatingAndCoolingSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_THERMOSTAT);
    } else if (isDrySupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_DRYER);
    } else if (isFanSupported()) {
      setAndSaveFunction(SUPLA_CHANNELFNC_HVAC_FAN);
    }
    initDefaultConfig();
  }

  initDefaultAlgorithm();

  if (!isWeeklyScheduleConfigured) {
    initDefaultWeeklySchedule();
  }

  initDone = true;

  // validate thermometers channel numbers
  if (!setMainThermometerChannelNo(getMainThermometerChannelNo())) {
    SUPLA_LOG_WARNING(
        "HVAC[%d]: main thermometer channel number %d is invalid. Clearing.",
        getChannelNumber(),
        getMainThermometerChannelNo());
    setMainThermometerChannelNo(getChannelNumber());
  }
  if (!setAuxThermometerChannelNo(getAuxThermometerChannelNo())) {
    SUPLA_LOG_WARNING(
        "HVAC[%d]: aux thermomter channel number %d is invalid. Clearing.",
        getChannelNumber(),
        getAuxThermometerChannelNo());
    setAuxThermometerChannelNo(getChannelNumber());
  }
  if (!setBinarySensorChannelNo(getBinarySensorChannelNo())) {
    if (getBinarySensorChannelNo() == getChannelNumber()) {
      SUPLA_LOG_DEBUG("HVAC[%d]: binary sensor function disabled",
                      getChannelNumber());
    } else {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: binary sensor channel number %d is invalid. Clearing.",
          getChannelNumber(),
          getBinarySensorChannelNo());
      setBinarySensorChannelNo(getChannelNumber());
    }
  }

  if (channel.isHvacFlagWeeklySchedule()) {
    if (!processWeeklySchedule()) {
      return;
    }
  } else {
    channel.setHvacFlagWeeklyScheduleTemporalOverride(false);
  }

  uint8_t mode = channel.getHvacMode();
  if (mode == SUPLA_HVAC_MODE_NOT_SET) {
    turnOn();
    setOutput(0, true);
  }
//  debugPrintConfigStruct(&config, getChannelNumber());
  previousSubfunction = config.Subfunction;
}


void HvacBase::onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc) {
  SUPLA_LOG_DEBUG(
      "HVAC[%d]: onRegistered send: IsOn %d, Mode %s, tHeat %d, tCool %d, "
      "flags 0x%x",
      getChannelNumber(),
      channel.getHvacIsOn(),
      channel.getHvacModeCstr(),
      channel.getHvacSetpointTemperatureHeat(),
      channel.getHvacSetpointTemperatureCool(),
      channel.getHvacFlags());
  Supla::Element::onRegistered(suplaSrpc);
  serverChannelFunctionValid = true;
  configFinishedReceived = false;
  if (channelConfigChangedOffline) {
    channelConfigChangedOffline = 1;
  }
  if (weeklyScheduleChangedOffline) {
    weeklyScheduleChangedOffline = 1;
  }
  defaultConfigReceived = false;
  weeklyScheduleReceived = false;
  altWeeklyScheduleReceived = false;
  timerUpdateTimestamp = 0;
}

void HvacBase::handleChannelConfigFinished() {
  configFinishedReceived = true;
  if (!defaultConfigReceived) {
    // trigger sending channel config to server
    channelConfigChangedOffline = 1;
  }
  if (!weeklyScheduleReceived) {
    // trigger sending weekly schedule to server
    weeklyScheduleChangedOffline = 1;
  }
  if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
      !altWeeklyScheduleReceived) {
    // trigger sending weekly schedule to server
    weeklyScheduleChangedOffline = 1;
  }
}

void HvacBase::iterateAlways() {
  if (getChannelFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (config.Subfunction == SUPLA_HVAC_SUBFUNCTION_NOT_SET) {
      setSubfunction(SUPLA_HVAC_SUBFUNCTION_HEAT);
    }
  }

  if (previousSubfunction != config.Subfunction) {
    previousSubfunction = config.Subfunction;
    memset(&lastWorkingMode, 0, sizeof(lastWorkingMode));
    lastManualMode = 0;
    if (channel.isHvacFlagHeating() || channel.isHvacFlagCooling()) {
      runAction(Supla::ON_HVAC_STANDBY);
    }
    channel.clearHvacState();
    clearLastOutputValue();
    setOutput(0, true);
    fixTemperatureSetpoints();
    lastIterateTimestampMs = 0;
  }

  updateChannelState();

  if (lastIterateTimestampMs && millis() - lastIterateTimestampMs < 1000) {
    return;
  }
  lastIterateTimestampMs = millis();

  if (!registeredInRelayHvacAggregator) {
    bool pumpReady = true;
    if (isPumpSwitchSet()) {
      if (!registerInAggregator(getPumpSwitchChannelNo())) {
        pumpReady = false;
      }
    }
    bool heatOrColdReady = true;
    if (isHeatOrColdSourceSwitchSet()) {
      if (!registerInAggregator(getHeatOrColdSourceSwitchChannelNo())) {
        heatOrColdReady = false;
      }
    }
    registeredInRelayHvacAggregator = pumpReady && heatOrColdReady;
  }

  if (isMasterThermostatSet()) {
    auto masterCh =
        Supla::Channel::GetByChannelNumber(getMasterThermostatChannelNo());
    if (masterCh) {
      auto masterMode = masterCh->getHvacMode();
      auto myMode = channel.getHvacMode();
      auto masterSetpointHeat = masterCh->getHvacSetpointTemperatureHeat();
      auto masterSetpointCool = masterCh->getHvacSetpointTemperatureCool();
      auto mySetpointHeat = channel.getHvacSetpointTemperatureHeat();
      auto mySetpointCool = channel.getHvacSetpointTemperatureCool();

      switch (masterMode) {
        case SUPLA_HVAC_MODE_HEAT: {
          masterSetpointCool = INT16_MIN;
          mySetpointCool = INT16_MIN;
          break;
        }
        case SUPLA_HVAC_MODE_COOL: {
          masterSetpointHeat = INT16_MIN;
          mySetpointHeat = INT16_MIN;
          break;
        }
        case SUPLA_HVAC_MODE_OFF: {
          masterSetpointHeat = INT16_MIN;
          masterSetpointCool = INT16_MIN;
          mySetpointHeat = INT16_MIN;
          mySetpointCool = INT16_MIN;
          break;
        }
      }

      if (myMode != masterMode || mySetpointHeat != masterSetpointHeat ||
          mySetpointCool != masterSetpointCool) {
        SUPLA_LOG_INFO("HVAC[%d]: Master thermostat settings changed "
            "mode %d->%d, heat %d->%d, cool %d->%d",
                       getChannelNumber(),
                       myMode, masterMode,
                       mySetpointHeat, masterSetpointHeat,
                       mySetpointCool, masterSetpointCool);
        if (!applyNewRuntimeSettings(
                masterMode, masterSetpointHeat, masterSetpointCool)) {
          setTargetMode(SUPLA_HVAC_MODE_OFF);
        }
      }
    }
  }

  if (Supla::Clock::IsReady()) {
    channel.setHvacFlagClockError(false);
  } else {
    channel.setHvacFlagClockError(true);
  }

  if (isCoolingSubfunction()) {
    channel.setHvacFlagCoolSubfunction(
        HvacCoolSubfunctionFlag::CoolSubfunction);
  } else {
    channel.setHvacFlagCoolSubfunction(
        HvacCoolSubfunctionFlag::HeatSubfunctionOrNotUsed);
  }

  // update tempreatures information
  auto t1 = getPrimaryTemp();
  auto t2 = getSecondaryTemp();

  // wait with reaction to new settings
  if (lastConfigChangeTimestampMs &&
      millis() - lastConfigChangeTimestampMs < 5000) {
    return;
  }
  lastConfigChangeTimestampMs = 0;

  if (getChannel()->isHvacFlagCountdownTimer()) {
    if (countdownTimerEnds <= Supla::Clock::GetTimeStamp()) {
      getChannel()->setHvacFlagCountdownTimer(false);
      turnOn();  // turnOn restores last stored runtime mode
      countdownTimerEnds = 1;
    }
  }

  if (!isModeSupported(channel.getHvacMode())) {
    setTargetMode(SUPLA_HVAC_MODE_OFF);
  }

  if (channel.isHvacFlagWeeklySchedule()) {
    if (!processWeeklySchedule()) {
      return;
    }
  } else {
    channel.setHvacFlagWeeklyScheduleTemporalOverride(false);
  }

  if (!checkThermometersStatusForCurrentMode(t1, t2)) {
    setOutput(getOutputValueOnError(), true);
    if (lastTemperature != INT16_MIN) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: invalid temperature readout - check if your thermometer "
          "is "
          "correctly connected and configured",
          getChannelNumber());
    }
    lastTemperature = INT16_MIN;
    channel.setHvacFlagThermometerError(true);
    channel.setHvacFlagForcedOffBySensor(false);
    return;
  }
  channel.setHvacFlagThermometerError(false);

  if (channel.getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    t1 -= t2;
    t2 = INT16_MIN;
  }
  lastTemperature = t1;

  if (checkOverheatProtection(t1)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: overheat protection exit", getChannelNumber());
    return;
  }

  if (checkAntifreezeProtection(t1)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: antifreeze protection exit", getChannelNumber());
    return;
  }

  if (isOutputControlledInternally()) {
    if (getForcedOffSensorState()) {
      SUPLA_LOG_DEBUG("HVAC[%d]: forced off by sensor exit",
                      getChannelNumber());
      channel.setHvacFlagForcedOffBySensor(true);
      setOutput(0, false);
      return;
    } else {
      channel.setHvacFlagForcedOffBySensor(false);
    }
  } else {
    if (getForcedOffSensorState()) {
      if (channel.getHvacMode() != SUPLA_HVAC_MODE_OFF) {
        channel.setHvacFlagForcedOffBySensor(true);
        setTargetMode(SUPLA_HVAC_MODE_OFF);
        SUPLA_LOG_DEBUG("HVAC[%d]: forced off by sensor exit (with turn off)",
                        getChannelNumber());
        return;
      }
    } else {
      if (channel.isHvacFlagForcedOffBySensor()) {
        channel.setHvacFlagForcedOffBySensor(false);
        setTargetMode(SUPLA_HVAC_MODE_CMD_TURN_ON);
        SUPLA_LOG_DEBUG("HVAC[%d]: turn on by sensor state",
                        getChannelNumber());
        return;
      }
    }
  }

  if (checkAuxProtection(t2)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: heater/cooler protection exit",
                    getChannelNumber());
    return;
  }

  switch (channel.getHvacMode()) {
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      int heatNewOutputValue =
          evaluateHeatOutputValue(t1, getTemperatureSetpointHeat());
      int coolNewOutputValue =
          evaluateCoolOutputValue(t1, getTemperatureSetpointCool());
      if (heatNewOutputValue > 0) {
        setOutput(heatNewOutputValue, false);
      } else if (coolNewOutputValue < 0) {
        setOutput(coolNewOutputValue, false);
      } else {
        setOutput(0, false);
      }

      break;
    }
    case SUPLA_HVAC_MODE_HEAT: {
      int newOutputValue =
          evaluateHeatOutputValue(t1, getTemperatureSetpointHeat());
      if (newOutputValue < 0) {
        newOutputValue = 0;
      }
      setOutput(newOutputValue, false);
      break;
    }
    case SUPLA_HVAC_MODE_COOL: {
      int newOutputValue =
          evaluateCoolOutputValue(t1, getTemperatureSetpointCool());
      if (newOutputValue > 0) {
        newOutputValue = 0;
      }
      setOutput(newOutputValue, false);
      break;
    }
    case SUPLA_HVAC_MODE_OFF: {
      setOutput(0, false);
      break;
    }
    /*
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL: {
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DRYER:
    case SUPLA_CHANNELFNC_HVAC_FAN: {
      // not implemented yet
      break;
    }
     */
    default: {
      break;
    }
  }
}

void HvacBase::setHeatingAndCoolingSupported(bool supported) {
  if (supported) {
    channel.addToFuncList(SUPLA_BIT_FUNC_HVAC_THERMOSTAT);
  } else {
    channel.removeFromFuncList(SUPLA_BIT_FUNC_HVAC_THERMOSTAT);
    setHeatCoolSupported(false);
  }
}

void HvacBase::setHeatCoolSupported(bool supported) {
  if (supported) {
    channel.addToFuncList(SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL);
    setHeatingAndCoolingSupported(true);
  } else {
    channel.removeFromFuncList(SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL);
  }
}

void HvacBase::setFanSupported(bool supported) {
  (void)(supported);
  // not implemented yet
}

void HvacBase::setDrySupported(bool supported) {
  (void)(supported);
  // not implemented yet
}

bool HvacBase::isHeatingAndCoolingSupported() const {
  return channel.getFuncList() & SUPLA_BIT_FUNC_HVAC_THERMOSTAT;
}

bool HvacBase::isHeatCoolSupported() const {
  return channel.getFuncList() & SUPLA_BIT_FUNC_HVAC_THERMOSTAT_HEAT_COOL;
}

bool HvacBase::isFanSupported() const {
  return false;
}

bool HvacBase::isDrySupported() const {
  return false;
}

uint8_t HvacBase::handleChannelConfig(TSD_ChannelConfig *newConfig,
                                      bool local) {
  SUPLA_LOG_DEBUG("HVAC[%d]: processing channel config", getChannelNumber());
  if (newConfig == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto channelFunction = newConfig->Func;
  if (!isFunctionSupported(channelFunction)) {
    serverChannelFunctionValid = false;
    if (channelFunction == 0) {
      // Channel function set to "none/disabled"
      changeFunction(channelFunction, false);
      return SUPLA_CONFIG_RESULT_TRUE;
    }
    return SUPLA_CONFIG_RESULT_FUNCTION_NOT_SUPPORTED;
  }
  serverChannelFunctionValid = true;

  if (channelConfigChangedOffline && !local) {
    SUPLA_LOG_INFO(
        "HVAC[%d]: Ignoring channel config (local config changed offline)",
        getChannelNumber());
    defaultConfigReceived = true;
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (newConfig->ConfigType != SUPLA_CONFIG_TYPE_DEFAULT) {
    return SUPLA_CONFIG_RESULT_TYPE_NOT_SUPPORTED;
  }

  if (channelFunction != getChannel()->getDefaultFunction()) {
    SUPLA_LOG_INFO("HVAC[%d]: function changed to %d",
                   getChannelNumber(),
                   channelFunction);
    changeFunction(channelFunction, false);
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  bool applyServerConfig = (newConfig->ConfigSize > 0);
  if (!applyServerConfig) {
    // server doesn't have channel configuration, so we'll send it
    channelConfigChangedOffline = 1;
  }

  if (applyServerConfig &&
      newConfig->ConfigSize < sizeof(TChannelConfig_HVAC)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  TChannelConfig_HVAC *hvacConfig = nullptr;
  bool readonlyChanged = false;
  bool additionalValidationChanged = false;
  if (applyServerConfig) {
    hvacConfig = reinterpret_cast<TChannelConfig_HVAC *>(newConfig->Config);
    readonlyChanged = fixReadonlyParameters(hvacConfig);
    additionalValidationChanged = applyAdditionalValidation(hvacConfig);
    debugPrintConfigDiff(&config, hvacConfig, getChannelNumber());
  }

  if (applyServerConfig && !isConfigValid(hvacConfig)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid config", getChannelNumber());
    // server have invalid channel config
    if (!configFinishedReceived) {
      // if first config after register is invalid, we try to send out config
      // to server in order to fix it. If next channel configs will be also
      // invalid, we reject them without sending out config to server to avoid
      // message infinite loop
      channelConfigChangedOffline = 1;
    }
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  TChannelConfig_HVAC configCopy;
  memcpy(&configCopy, &config, sizeof(TChannelConfig_HVAC));

  // Received config looks ok, so we apply it to channel
  if (applyServerConfig) {
    if (config.PumpSwitchChannelNo != hvacConfig->PumpSwitchChannelNo) {
      unregisterInAggregator(config.PumpSwitchChannelNo);
    }
    if (config.HeatOrColdSourceSwitchChannelNo !=
        hvacConfig->HeatOrColdSourceSwitchChannelNo) {
      unregisterInAggregator(config.HeatOrColdSourceSwitchChannelNo);
    }
    registeredInRelayHvacAggregator = false;
    registerInAggregator(hvacConfig->PumpSwitchChannelNo);
    registerInAggregator(hvacConfig->HeatOrColdSourceSwitchChannelNo);
    applyConfigWithoutValidation(hvacConfig);
    fixTempearturesConfig();
    fixTemperatureSetpoints();
  }

  defaultConfigReceived = true;
  if (memcmp(&config, &configCopy, sizeof(TChannelConfig_HVAC)) != 0) {
    if (local && initDone) {
      channelConfigChangedOffline = 1;
    }
    saveConfig();
  }

  // check if readonly fields have correct values on server
  // if not, then send update to server
  // Check is done only on first config after registration
  if (readonlyChanged || additionalValidationChanged) {
    channelConfigChangedOffline = 1;
  }

  return SUPLA_CONFIG_RESULT_TRUE;
}

void HvacBase::applyConfigWithoutValidation(TChannelConfig_HVAC *hvacConfig) {
  if (hvacConfig == nullptr) {
    return;
  }
  // We don't use setters here, because they run validation against current
  // configuration, which may fail. However new config was already validated
  // so we assign them directly.
  config.MainThermometerChannelNo = hvacConfig->MainThermometerChannelNo;
  config.AuxThermometerChannelNo = hvacConfig->AuxThermometerChannelNo;
  config.BinarySensorChannelNo = hvacConfig->BinarySensorChannelNo;
  config.AuxThermometerType = hvacConfig->AuxThermometerType;
  config.AntiFreezeAndOverheatProtectionEnabled =
    hvacConfig->AntiFreezeAndOverheatProtectionEnabled;
  config.UsedAlgorithm = hvacConfig->UsedAlgorithm;
  config.MinOnTimeS = hvacConfig->MinOnTimeS;
  config.MinOffTimeS = hvacConfig->MinOffTimeS;
  config.OutputValueOnError = hvacConfig->OutputValueOnError;
  config.Subfunction = hvacConfig->Subfunction;
  config.TemperatureSetpointChangeSwitchesToManualMode =
      hvacConfig->TemperatureSetpointChangeSwitchesToManualMode;
  config.AuxMinMaxSetpointEnabled = hvacConfig->AuxMinMaxSetpointEnabled;
  config.UseSeparateHeatCoolOutputs = hvacConfig->UseSeparateHeatCoolOutputs;
  config.PumpSwitchIsSet = hvacConfig->PumpSwitchIsSet;
  config.PumpSwitchChannelNo = hvacConfig->PumpSwitchChannelNo;
  config.HeatOrColdSourceSwitchIsSet = hvacConfig->HeatOrColdSourceSwitchIsSet;
  config.HeatOrColdSourceSwitchChannelNo =
      hvacConfig->HeatOrColdSourceSwitchChannelNo;
  config.MasterThermostatIsSet = hvacConfig->MasterThermostatIsSet;
  config.MasterThermostatChannelNo = hvacConfig->MasterThermostatChannelNo;

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures, TEMPERATURE_ECO)) {
    setTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_ECO,
        getTemperatureEco(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures, TEMPERATURE_ECO);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_COMFORT)) {
    setTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_COMFORT,
        getTemperatureComfort(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures, TEMPERATURE_COMFORT);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_BOOST)) {
    setTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_BOOST,
        getTemperatureBoost(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures, TEMPERATURE_BOOST);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_FREEZE_PROTECTION)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_FREEZE_PROTECTION,
        getTemperatureFreezeProtection(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_FREEZE_PROTECTION);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_HEAT_PROTECTION)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_HEAT_PROTECTION,
        getTemperatureHeatProtection(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_HEAT_PROTECTION);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_HISTERESIS)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_HISTERESIS,
        getTemperatureHisteresis(&hvacConfig->Temperatures));
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_BELOW_ALARM)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_BELOW_ALARM,
        getTemperatureBelowAlarm(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_BELOW_ALARM);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_ABOVE_ALARM)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_ABOVE_ALARM,
        getTemperatureAboveAlarm(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_ABOVE_ALARM);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_AUX_MIN_SETPOINT)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_AUX_MIN_SETPOINT,
        getTemperatureAuxMinSetpoint(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_AUX_MIN_SETPOINT);
  }

  if (isTemperatureSetInStruct(&hvacConfig->Temperatures,
        TEMPERATURE_AUX_MAX_SETPOINT)) {
    setTemperatureInStruct(
        &config.Temperatures,
        TEMPERATURE_AUX_MAX_SETPOINT,
        getTemperatureAuxMaxSetpoint(&hvacConfig->Temperatures));
  } else {
    clearTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_AUX_MAX_SETPOINT);
  }
}

bool HvacBase::isConfigValid(TChannelConfig_HVAC *newConfig) const {
  if (newConfig == nullptr) {
    return false;
  }

  // heater cooler thermometer is optional, but if set, it has to be set to a
  // local thermometer
  if (newConfig->AuxThermometerType !=
      SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET) {
    if (!isChannelThermometer(newConfig->AuxThermometerChannelNo)) {
      return false;
    }
    if (newConfig->AuxThermometerChannelNo ==
        newConfig->MainThermometerChannelNo) {
      return false;
    }
  }

  switch (newConfig->AuxThermometerType) {
    case SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET:
    case SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED:
    case SUPLA_HVAC_AUX_THERMOMETER_TYPE_FLOOR:
    case SUPLA_HVAC_AUX_THERMOMETER_TYPE_WATER:
    case SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_COOLER:
    case SUPLA_HVAC_AUX_THERMOMETER_TYPE_GENERIC_HEATER:
      break;
    default:
      SUPLA_LOG_WARNING("HVAC[%d]: invalid aux thermometer type %d",
                        channel.getChannelNumber(),
                        newConfig->AuxThermometerType);
      return false;
  }

  if (newConfig->BinarySensorChannelNo != channel.getChannelNumber()) {
    if (!isChannelBinarySensor(newConfig->BinarySensorChannelNo)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid binary sensor %d",
                        channel.getChannelNumber(),
                        newConfig->BinarySensorChannelNo);
      return false;
    }
  }

  if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    switch (newConfig->Subfunction) {
      case SUPLA_HVAC_SUBFUNCTION_NOT_SET:
      case SUPLA_HVAC_SUBFUNCTION_COOL:
      case SUPLA_HVAC_SUBFUNCTION_HEAT:
        break;
      default: {
        SUPLA_LOG_WARNING("HVAC[%d]: invalid subfunction %d",
                          channel.getChannelNumber(),
                          newConfig->Subfunction);
        return false;
      }
    }
  }

  if (!isAlgorithmValid(newConfig->UsedAlgorithm)) {
    SUPLA_LOG_WARNING("HVAC[%d]: invalid algorithm %d",
                      channel.getChannelNumber(),
                      newConfig->UsedAlgorithm);
    return false;
  }

  if (!isMinOnOffTimeValid(newConfig->MinOnTimeS)) {
    SUPLA_LOG_WARNING("HVAC[%d]: invalid min on time %d",
                      channel.getChannelNumber(),
                      newConfig->MinOnTimeS);
    return false;
  }

  if (!isMinOnOffTimeValid(newConfig->MinOffTimeS)) {
    SUPLA_LOG_WARNING("HVAC[%d]: invalid min off time %d",
                      channel.getChannelNumber(),
                      newConfig->MinOffTimeS);
    return false;
  }

  if (newConfig->OutputValueOnError > 100 ||
      newConfig->OutputValueOnError < -100) {
    SUPLA_LOG_WARNING("HVAC[%d]: invalid output value on error %d",
                      channel.getChannelNumber(),
                      newConfig->OutputValueOnError);
    return false;
  }

  if (!areTemperaturesValid(&newConfig->Temperatures)) {
    SUPLA_LOG_WARNING("HVAC[%d]: invalid temperatures",
                      channel.getChannelNumber());
    return false;
  }

  return true;
}

bool HvacBase::areTemperaturesValid(
    const THVACTemperatureCfg *temperatures) const {
  if (temperatures == nullptr) {
    return false;
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_FREEZE_PROTECTION)) {
    if (!isTemperatureFreezeProtectionValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid freeze protection temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_HEAT_PROTECTION)) {
    if (!isTemperatureHeatProtectionValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid heat protection temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_ECO)) {
    if (!isTemperatureEcoValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid eco temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_COMFORT)) {
    if (!isTemperatureComfortValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid comfort temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_BOOST)) {
    if (!isTemperatureBoostValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid boost temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_HISTERESIS)) {
    if (!isTemperatureHisteresisValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid histeresis value",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_BELOW_ALARM)) {
    if (!isTemperatureBelowAlarmValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid below alarm temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures, TEMPERATURE_ABOVE_ALARM)) {
    if (!isTemperatureAboveAlarmValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid above alarm temperature",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures,
                               TEMPERATURE_AUX_MIN_SETPOINT)) {
    if (!isTemperatureAuxMinSetpointValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid aux min setpoint",
                        channel.getChannelNumber());
      return false;
    }
  }

  if (isTemperatureSetInStruct(temperatures,
                               TEMPERATURE_AUX_MAX_SETPOINT)) {
    if (!isTemperatureAuxMaxSetpointValid(temperatures)) {
      SUPLA_LOG_WARNING("HVAC[%d]: invalid aux max setpoint");
      return false;
    }
  }

  return true;
}

bool HvacBase::isTemperatureSetInStruct(const THVACTemperatureCfg *temperatures,
                                        unsigned _supla_int_t index) {
  if (temperatures == nullptr) {
    return false;
  }

  if (index == 0) {
    return false;
  }

  return (temperatures->Index & index) == index;
}

int32_t HvacBase::getArrayIndex(int32_t bitIndex) {
  // convert index bit number to array index
  int32_t arrayIndex = 0;
  for (; arrayIndex < 24; arrayIndex++) {
    if (bitIndex & (1 << arrayIndex)) {
      return arrayIndex;
    }
  }
  return -1;
}

_supla_int16_t HvacBase::getTemperatureFromStruct(
    const THVACTemperatureCfg *temperatures, unsigned _supla_int_t index) {
  if (!isTemperatureSetInStruct(temperatures, index)) {
    return SUPLA_TEMPERATURE_INVALID_INT16;
  }

  int32_t arrayIndex = getArrayIndex(index);
  if (arrayIndex < 0) {
    return INT16_MIN;
  }
  return temperatures->Temperature[arrayIndex];
}

bool HvacBase::isTemperatureInRoomConstrain(_supla_int16_t temperature) const {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureInHeatCoolConstrain(_supla_int16_t tHeat,
                                            _supla_int16_t tCool) const {
  auto offsetMin = getTemperatureHeatCoolOffsetMin();
  auto offsetMax = getTemperatureHeatCoolOffsetMax();
  return (tCool - tHeat >= offsetMin) && (tCool - tHeat <= offsetMax) &&
         isTemperatureInRoomConstrain(tHeat) &&
         isTemperatureInRoomConstrain(tCool);
}

bool HvacBase::isTemperatureInAuxConstrain(
    _supla_int16_t temperature) const {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin = getTemperatureFromStruct(&config.Temperatures,
                                       TEMPERATURE_AUX_MIN);
  auto tMax = getTemperatureFromStruct(&config.Temperatures,
                                       TEMPERATURE_AUX_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureFreezeProtectionValid(
    _supla_int16_t temperature) const {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureFreezeProtectionValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t =
      getTemperatureFromStruct(temperatures, TEMPERATURE_FREEZE_PROTECTION);
  return isTemperatureFreezeProtectionValid(t);
}

bool HvacBase::isTemperatureHeatProtectionValid(
    _supla_int16_t temperature) const {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureHeatProtectionValid(
    const THVACTemperatureCfg *temperatures) const {
  auto  t = getTemperatureFromStruct(temperatures, TEMPERATURE_HEAT_PROTECTION);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureEcoValid(_supla_int16_t temperature) const {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureEcoValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_ECO);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureComfortValid(_supla_int16_t temperature) const {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureComfortValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_COMFORT);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureBoostValid(_supla_int16_t temperature) const {
  return isTemperatureInRoomConstrain(temperature);
}

bool HvacBase::isTemperatureBoostValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_BOOST);
  return isTemperatureInRoomConstrain(t);
}

bool HvacBase::isTemperatureHisteresisValid(_supla_int16_t hist) const {
  if (hist == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto histMin = getTemperatureFromStruct(&config.Temperatures,
                                          TEMPERATURE_HISTERESIS_MIN);
  auto histMax = getTemperatureFromStruct(&config.Temperatures,
                                          TEMPERATURE_HISTERESIS_MAX);

  return hist >= histMin && hist <= histMax;
}

bool HvacBase::isTemperatureHisteresisValid(
    const THVACTemperatureCfg *temperatures) const {
  auto hist = getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS);
  return isTemperatureHisteresisValid(hist);
}

bool HvacBase::isTemperatureAuxMinSetpointValid(
    _supla_int16_t temperature) const {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin = getTemperatureAuxMin();
  auto tMax = getTemperatureAuxMax();
  auto tOffsetMin = getTemperatureHeatCoolOffsetMin();

  auto tMaxSetpoint = getTemperatureAuxMaxSetpoint();

  return (temperature <= (tMaxSetpoint - tOffsetMin) ||
          tMaxSetpoint == SUPLA_TEMPERATURE_INVALID_INT16) &&
         temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureAuxMinSetpointValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureAuxMinSetpoint(temperatures);

  auto tMin = getTemperatureAuxMin();
  auto tMax = getTemperatureAuxMax();

  return t >= tMin && t <= tMax;
}

bool HvacBase::isTemperatureAuxMaxSetpointValid(
    _supla_int16_t temperature) const {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  // min and max values are taken from device config
  auto tMin = getTemperatureAuxMin();
  auto tMax = getTemperatureAuxMax();
  auto tOffsetMin = getTemperatureHeatCoolOffsetMin();

  // current min setpoint is taken from current device config
  auto tMinSetpoint = getTemperatureAuxMinSetpoint();

  return (temperature >= (tMinSetpoint + tOffsetMin) ||
          tMinSetpoint == SUPLA_TEMPERATURE_INVALID_INT16) &&
         temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureAuxMaxSetpointValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureAuxMaxSetpoint(temperatures);

  // min and max range is validated against device config (readonly values)
  auto tMin = getTemperatureAuxMin();
  auto tMax = getTemperatureAuxMax();

  return t >= tMin && t <= tMax;
}

bool HvacBase::isTemperatureBelowAlarmValid(_supla_int16_t temperature) const {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }

  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureBelowAlarmValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_BELOW_ALARM);
  return isTemperatureBelowAlarmValid(t);
}

bool HvacBase::isTemperatureAboveAlarmValid(_supla_int16_t temperature) const {
  if (temperature == SUPLA_TEMPERATURE_INVALID_INT16) {
    return false;
  }
  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  return temperature >= tMin && temperature <= tMax;
}

bool HvacBase::isTemperatureAboveAlarmValid(
    const THVACTemperatureCfg *temperatures) const {
  auto t = getTemperatureFromStruct(temperatures, TEMPERATURE_ABOVE_ALARM);
  return isTemperatureAboveAlarmValid(t);
}

bool HvacBase::isChannelThermometer(uint8_t channelNo) const {
  if (getChannelNumber() == channelNo) {
    // skip checking for self (Hvac)
    return false;
  }
  auto element = Supla::Element::getElementByChannelNumber(channelNo);
  if (element == nullptr) {
    SUPLA_LOG_WARNING("HVAC[%d]: thermometer not found for channel %d",
                      getChannelNumber(),
                      channelNo);
    return false;
  }
  auto elementType = element->getChannel()->getChannelType();
  switch (elementType) {
    case SUPLA_CHANNELTYPE_HUMIDITYANDTEMPSENSOR:
    case SUPLA_CHANNELTYPE_THERMOMETER:
      break;
    default:
      SUPLA_LOG_WARNING("HVAC[%d]: thermometer channel %d has invalid type %d",
                        getChannelNumber(),
          channelNo, elementType);
      return false;
  }
  return true;
}

bool HvacBase::isChannelBinarySensor(uint8_t channelNo) const {
  auto element = Supla::Element::getElementByChannelNumber(channelNo);
  if (element == nullptr) {
    SUPLA_LOG_WARNING("HVAC[%d]: binary sensor not found for channel %d",
                      getChannelNumber(),
                      channelNo);
    return false;
  }
  if (element->getChannel()->getChannelType() ==
      SUPLA_CHANNELTYPE_BINARYSENSOR) {
    return true;
  }
  return false;
}

bool HvacBase::isAlgorithmValid(unsigned _supla_int16_t algorithm) const {
  return (config.AvailableAlgorithms & algorithm) == algorithm;
}

uint8_t HvacBase::handleWeeklySchedule(TSD_ChannelConfig *newWeeklySchedule,
                                       bool isAltWeeklySchedule,
                                       bool local) {
  SUPLA_LOG_DEBUG("HVAC[%d]: Handling weekly schedule",
      getChannelNumber());
  if (weeklyScheduleChangedOffline) {
    SUPLA_LOG_INFO("HVAC[%d]: Ignoring%s weekly schedule",
                   getChannelNumber(),
                   isAltWeeklySchedule ? " alt" : "");
    if (isAltWeeklySchedule) {
      altWeeklyScheduleReceived = true;
    } else {
      weeklyScheduleReceived = true;
    }
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (newWeeklySchedule == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  if (newWeeklySchedule->ConfigSize == 0) {
    // Empty config for weekly schedule means that no weekly schedule is
    // configured
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: Missing weekly schedule on server. Sending local schedule",
        getChannelNumber());
    if (!isWeeklyScheduleConfigured) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d]: No weekly schedule configured. Using SW defaults.",
          getChannelNumber());
      initDefaultWeeklySchedule();
    }
    weeklyScheduleChangedOffline = 1;
    return SUPLA_CONFIG_RESULT_TRUE;
  }

  if (newWeeklySchedule->ConfigSize < sizeof(TChannelConfig_WeeklySchedule)) {
    SUPLA_LOG_WARNING("HVAC[%d]: Invalid weekly schedule",
                     getChannelNumber());
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto newSchedule = reinterpret_cast<TChannelConfig_WeeklySchedule *>(
      newWeeklySchedule->Config);

  if (!isWeeklyScheduleValid(newSchedule, isAltWeeklySchedule)) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }

  auto weeklySchedulePtr =
      (isAltWeeklySchedule ? &altWeeklySchedule : &weeklySchedule);

  if (isAltWeeklySchedule) {
    altWeeklyScheduleReceived = true;
  } else {
    weeklyScheduleReceived = true;
  }

  if (!isWeeklyScheduleConfigured || memcmp(weeklySchedulePtr,
             newSchedule,
             sizeof(TChannelConfig_WeeklySchedule)) != 0) {
    memcpy(weeklySchedulePtr,
           newSchedule,
           sizeof(TChannelConfig_WeeklySchedule));
    isWeeklyScheduleConfigured = true;
    if (!local) {
      weeklyScheduleChangedOffline = 0;
    }
    saveWeeklySchedule();
  }

  return SUPLA_RESULT_TRUE;
}

bool HvacBase::isFunctionSupported(_supla_int_t channelFunction) const {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT:
      return isHeatingAndCoolingSupported();
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL:
      return isHeatCoolSupported();
    case SUPLA_CHANNELFNC_HVAC_FAN:
      return isFanSupported();
    case SUPLA_CHANNELFNC_HVAC_DRYER:
      return isDrySupported();
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL:
      return isDifferentialFunctionSupported();
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      return isDomesticHotWaterFunctionSupported();
    }
  }
  return false;
}

void HvacBase::addAvailableAlgorithm(unsigned _supla_int16_t algorithm) {
  if (initialConfig && !initDone) {
    initialConfig->AvailableAlgorithms |= algorithm;
  }
  config.AvailableAlgorithms |= algorithm;
}

void HvacBase::removeAvailableAlgorithm(unsigned _supla_int16_t algorithm) {
  if (initialConfig && !initDone) {
    initialConfig->AvailableAlgorithms &= ~algorithm;
  }
  config.AvailableAlgorithms &= ~algorithm;
}

void HvacBase::setTemperatureInStruct(THVACTemperatureCfg *temperatures,
                                      unsigned _supla_int_t index,
                                      _supla_int16_t temperature) {
  if (temperatures == nullptr) {
    return;
  }

  // convert index bit number to array index
  int32_t arrayIndex = getArrayIndex(index);

  if (arrayIndex < 0) {
    return;
  }

  temperatures->Index |= index;
  temperatures->Temperature[arrayIndex] = temperature;
}

void HvacBase::clearTemperatureInStruct(THVACTemperatureCfg *temperatures,
                              unsigned _supla_int_t index) {
    if (temperatures == nullptr) {
      return;
  }

  int32_t arrayIndex = getArrayIndex(index);
  if (arrayIndex < 0) {
    return;
  }
  temperatures->Index &= ~index;
  temperatures->Temperature[arrayIndex] = 0;
}

void HvacBase::setTemperatureRoomMin(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_ROOM_MIN, temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_ROOM_MIN, temperature);
}

void HvacBase::setTemperatureRoomMax(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_ROOM_MAX, temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_ROOM_MAX, temperature);
}

void HvacBase::setTemperatureAuxMin(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_AUX_MIN, temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_AUX_MIN, temperature);
}

void HvacBase::setTemperatureAuxMax(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_AUX_MAX, temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_AUX_MAX, temperature);
}

void HvacBase::setTemperatureHisteresisMin(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_HISTERESIS_MIN, temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HISTERESIS_MIN, temperature);
}

void HvacBase::setTemperatureHisteresisMax(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_HISTERESIS_MAX, temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HISTERESIS_MAX, temperature);
}

void HvacBase::setTemperatureHeatCoolOffsetMin(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_HEAT_COOL_OFFSET_MIN,
      temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HEAT_COOL_OFFSET_MIN, temperature);
}

void HvacBase::setTemperatureHeatCoolOffsetMax(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(
      &initialConfig->Temperatures, TEMPERATURE_HEAT_COOL_OFFSET_MAX,
      temperature);
  }
  setTemperatureInStruct(
      &config.Temperatures, TEMPERATURE_HEAT_COOL_OFFSET_MAX, temperature);
}


bool HvacBase::setTemperatureFreezeProtection(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_FREEZE_PROTECTION,
                           temperature);
  }
  if (!isTemperatureFreezeProtectionValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureFreezeProtection()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_FREEZE_PROTECTION, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureHeatProtection(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_HEAT_PROTECTION,
                           temperature);
  }
  if (!isTemperatureHeatProtectionValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureHeatProtection()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_HEAT_PROTECTION, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureEco(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_ECO,
                           temperature);
  }
  if (!isTemperatureEcoValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureEco()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_ECO, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureComfort(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_COMFORT,
                           temperature);
  }
  if (!isTemperatureComfortValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureComfort()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_COMFORT, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureBoost(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_BOOST,
                           temperature);
  }
  if (!isTemperatureBoostValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureBoost()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_BOOST, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureHisteresis(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_HISTERESIS,
                           temperature);
  }
  if (!isTemperatureHisteresisValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureHisteresis()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_HISTERESIS, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureBelowAlarm(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_BELOW_ALARM,
                           temperature);
  }
  if (!isTemperatureBelowAlarmValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureBelowAlarm()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_BELOW_ALARM, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureAboveAlarm(_supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_ABOVE_ALARM,
                           temperature);
  }
  if (!isTemperatureAboveAlarmValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureAboveAlarm()) {
    setTemperatureInStruct(
        &config.Temperatures, TEMPERATURE_ABOVE_ALARM, temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureAuxMinSetpoint(
    _supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_AUX_MIN_SETPOINT,
                           temperature);
  }
  if (!isTemperatureAuxMinSetpointValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureAuxMinSetpoint()) {
    setTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_AUX_MIN_SETPOINT,
        temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

bool HvacBase::setTemperatureAuxMaxSetpoint(
    _supla_int16_t temperature) {
  if (initialConfig && !initDone) {
    setTemperatureInStruct(&(initialConfig->Temperatures),
                           TEMPERATURE_AUX_MAX_SETPOINT,
                           temperature);
  }
  if (!isTemperatureAuxMaxSetpointValid(temperature)) {
    return false;
  }
  if (temperature != getTemperatureAuxMaxSetpoint()) {
    setTemperatureInStruct(&config.Temperatures,
        TEMPERATURE_AUX_MAX_SETPOINT,
        temperature);
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

_supla_int16_t HvacBase::getTemperatureRoomMin(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ROOM_MIN);
}

_supla_int16_t HvacBase::getTemperatureRoomMax(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ROOM_MAX);
}

_supla_int16_t HvacBase::getTemperatureAuxMin(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_AUX_MIN);
}

_supla_int16_t HvacBase::getTemperatureAuxMax(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_AUX_MAX);
}
_supla_int16_t HvacBase::getTemperatureHisteresisMin(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS_MIN);
}
_supla_int16_t HvacBase::getTemperatureHisteresisMax(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS_MAX);
}
_supla_int16_t HvacBase::getTemperatureHeatCoolOffsetMin(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures,
                                  TEMPERATURE_HEAT_COOL_OFFSET_MIN);
}
_supla_int16_t HvacBase::getTemperatureHeatCoolOffsetMax(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures,
                                  TEMPERATURE_HEAT_COOL_OFFSET_MAX);
}

_supla_int16_t HvacBase::getTemperatureRoomMin() const {
  return getTemperatureRoomMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureRoomMax() const {
  return getTemperatureRoomMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAuxMin() const {
  return getTemperatureAuxMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAuxMax() const {
  return getTemperatureAuxMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHisteresisMin() const {
  return getTemperatureHisteresisMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHisteresisMax() const {
  return getTemperatureHisteresisMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeatCoolOffsetMin() const {
  return getTemperatureHeatCoolOffsetMin(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeatCoolOffsetMax() const {
  return getTemperatureHeatCoolOffsetMax(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureFreezeProtection(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_FREEZE_PROTECTION);
}

_supla_int16_t HvacBase::getTemperatureHeatProtection(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_HEAT_PROTECTION);
}

_supla_int16_t HvacBase::getTemperatureEco(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ECO);
}

_supla_int16_t HvacBase::getTemperatureComfort(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_COMFORT);
}

_supla_int16_t HvacBase::getTemperatureBoost(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_BOOST);
}

_supla_int16_t HvacBase::getTemperatureHisteresis(
    const THVACTemperatureCfg *temperatures) const {
  auto histeresis =
      getTemperatureFromStruct(temperatures, TEMPERATURE_HISTERESIS);
  return histeresis;
}

_supla_int16_t HvacBase::getTemperatureBelowAlarm(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_BELOW_ALARM);
}

_supla_int16_t HvacBase::getTemperatureAboveAlarm(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures, TEMPERATURE_ABOVE_ALARM);
}

_supla_int16_t HvacBase::getTemperatureAuxMinSetpoint(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures,
                                  TEMPERATURE_AUX_MIN_SETPOINT);
}

_supla_int16_t HvacBase::getTemperatureAuxMaxSetpoint(
    const THVACTemperatureCfg *temperatures) const {
  return getTemperatureFromStruct(temperatures,
                                  TEMPERATURE_AUX_MAX_SETPOINT);
}

_supla_int16_t HvacBase::getTemperatureFreezeProtection() const {
  return getTemperatureFreezeProtection(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHeatProtection() const {
  return getTemperatureHeatProtection(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureEco() const {
  return getTemperatureEco(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureComfort() const {
  return getTemperatureComfort(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureBoost() const {
  return getTemperatureBoost(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureHisteresis() const {
  return getTemperatureHisteresis(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureBelowAlarm() const {
  return getTemperatureBelowAlarm(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAboveAlarm() const {
  return getTemperatureAboveAlarm(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAuxMinSetpoint() const {
  return getTemperatureAuxMinSetpoint(&config.Temperatures);
}

_supla_int16_t HvacBase::getTemperatureAuxMaxSetpoint() const {
  return getTemperatureAuxMaxSetpoint(&config.Temperatures);
}

bool HvacBase::setUsedAlgorithm(unsigned _supla_int16_t newAlgorithm) {
  if (initialConfig && !initDone) {
    initialConfig->UsedAlgorithm = newAlgorithm;
  }
  if (isAlgorithmValid(newAlgorithm)) {
    if (config.UsedAlgorithm != newAlgorithm) {
      config.UsedAlgorithm = newAlgorithm;
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
    return true;
  }
  return false;
}

void HvacBase::setSubfunction(uint8_t subfunction) {
  if (initialConfig && !initDone) {
    initialConfig->Subfunction = subfunction;
  }
  if (config.Subfunction != subfunction) {
    config.Subfunction = subfunction;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
}

unsigned _supla_int16_t HvacBase::getUsedAlgorithm() const {
  return config.UsedAlgorithm;
}

bool HvacBase::setMainThermometerChannelNo(uint8_t channelNo) {
  SUPLA_LOG_INFO(" ********** SET MAIN THERMOMETER CHANNEL %d", channelNo);
  if (initialConfig && !initDone) {
    initialConfig->MainThermometerChannelNo = channelNo;
  }
  if (!initDone) {
    config.MainThermometerChannelNo = channelNo;
    defaultMainThermometer = channelNo;
    return true;
  }
  if (channelNo == getChannelNumber()) {
    if (config.MainThermometerChannelNo != channelNo) {
      config.MainThermometerChannelNo = channelNo;
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
  } else if (isChannelThermometer(channelNo)) {
    if (getAuxThermometerType() !=
        SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET) {
      if (channelNo == getAuxThermometerChannelNo()) {
        return false;
      }
    }
    if (config.MainThermometerChannelNo != channelNo) {
      config.MainThermometerChannelNo = channelNo;
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
    return true;
  }
  return true;
}

uint8_t HvacBase::getMainThermometerChannelNo() const {
  return config.MainThermometerChannelNo;
}

bool HvacBase::setAuxThermometerChannelNo(uint8_t channelNo) {
  if (initialConfig && !initDone) {
    initialConfig->AuxThermometerChannelNo = channelNo;
  }
  if (!initDone) {
    config.AuxThermometerChannelNo = channelNo;
    defaultAuxThermometer = channelNo;
    return true;
  }
  if (isChannelThermometer(channelNo)) {
    if (getMainThermometerChannelNo() == channelNo) {
      return false;
    }
    if (config.AuxThermometerChannelNo != channelNo) {
      config.AuxThermometerChannelNo = channelNo;
      if (getAuxThermometerType() ==
          SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET) {
        setAuxThermometerType(
            SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED);
        if (initDone) {
          channelConfigChangedOffline = 1;
          saveConfig();
        }
      }
    }
    return true;
  }

  if (getChannelNumber() == channelNo) {
    if (config.AuxThermometerChannelNo != channelNo) {
      config.AuxThermometerChannelNo = channelNo;
      setAuxThermometerType(
          SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET);
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
    return true;
  }
  return false;
}

uint8_t HvacBase::getAuxThermometerChannelNo() const {
  return config.AuxThermometerChannelNo;
}

void HvacBase::setAuxThermometerType(uint8_t type) {
  if (initialConfig && !initDone) {
    initialConfig->AuxThermometerType = type;
  }
  if (config.AuxThermometerType != type) {
    config.AuxThermometerType = type;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
}

uint8_t HvacBase::getAuxThermometerType() const {
  return config.AuxThermometerType;
}

void HvacBase::setAntiFreezeAndHeatProtectionEnabled(bool enabled) {
  if (initialConfig && !initDone) {
    initialConfig->AntiFreezeAndOverheatProtectionEnabled = enabled;
  }
  if (config.AntiFreezeAndOverheatProtectionEnabled != enabled) {
    config.AntiFreezeAndOverheatProtectionEnabled = enabled;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
}

bool HvacBase::isAntiFreezeAndHeatProtectionEnabled() const {
  auto func = channel.getDefaultFunction();
  switch (func) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT:
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL:
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      return config.AntiFreezeAndOverheatProtectionEnabled;
    }

    default: {
      return false;
    }
  }
}

void HvacBase::setAuxMinMaxSetpointEnabled(bool enabled) {
  if (initialConfig && !initDone) {
    initialConfig->AuxMinMaxSetpointEnabled = enabled;
  }
  if (config.AuxMinMaxSetpointEnabled != enabled) {
    config.AuxMinMaxSetpointEnabled = enabled;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
}

bool HvacBase::isAuxMinMaxSetpointEnabled() const {
  auto func = channel.getDefaultFunction();
  switch (func) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT:
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL:
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      return config.AuxMinMaxSetpointEnabled;
    }

    default: {
      return false;
    }
  }
}

void HvacBase::setTemperatureSetpointChangeSwitchesToManualMode(bool enabled) {
  if (initialConfig && !initDone) {
    initialConfig->TemperatureSetpointChangeSwitchesToManualMode = enabled;
  }
  if (config.TemperatureSetpointChangeSwitchesToManualMode != enabled) {
    config.TemperatureSetpointChangeSwitchesToManualMode = enabled;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
}

bool HvacBase::isTemperatureSetpointChangeSwitchesToManualMode() const {
  return config.TemperatureSetpointChangeSwitchesToManualMode;
}

void HvacBase::setUseSeparateHeatCoolOutputs(bool enabled) {
  if (initialConfig && !initDone) {
    initialConfig->UseSeparateHeatCoolOutputs = enabled;
  }
  if (config.UseSeparateHeatCoolOutputs != enabled) {
    config.UseSeparateHeatCoolOutputs = enabled;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
}

bool HvacBase::isUseSeparateHeatCoolOutputs() const {
  return config.UseSeparateHeatCoolOutputs;
}

bool HvacBase::isMinOnOffTimeValid(uint16_t seconds) const {
  return seconds <= 600;  // TODO(klew): is this range ok? from 1 s to 10 min
                          // 0 - disabled
}

bool HvacBase::setMinOnTimeS(uint16_t seconds) {
  if (initialConfig && !initDone) {
    initialConfig->MinOnTimeS = seconds;
  }
  if (isMinOnOffTimeValid(seconds)) {
    if (config.MinOnTimeS != seconds) {
      config.MinOnTimeS = seconds;
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
    return true;
  }
  return false;
}

uint16_t HvacBase::getMinOnTimeS() const {
  return config.MinOnTimeS;
}

bool HvacBase::setMinOffTimeS(uint16_t seconds) {
  if (initialConfig && !initDone) {
    initialConfig->MinOffTimeS = seconds;
  }
  if (isMinOnOffTimeValid(seconds)) {
    if (config.MinOffTimeS != seconds) {
      config.MinOffTimeS = seconds;
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
    return true;
  }
  return false;
}

uint16_t HvacBase::getMinOffTimeS() const {
  return config.MinOffTimeS;
}

bool HvacBase::setOutputValueOnError(signed char value) {
  if (value > 100) {
    value = 100;
  }
  if (value < -100) {
    value = -100;
  }
  if (initialConfig && !initDone) {
    initialConfig->OutputValueOnError = value;
  }

  if (config.OutputValueOnError != value) {
    config.OutputValueOnError = value;
    if (initDone) {
      channelConfigChangedOffline = 1;
      saveConfig();
    }
  }
  return true;
}

signed char HvacBase::getOutputValueOnError() const {
  return config.OutputValueOnError;
}

void HvacBase::saveConfig() {
  auto cfg = Supla::Storage::ConfigInstance();
  lastConfigChangeTimestampMs = millis();
  if (cfg) {
    // Generic HVAC configuration
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    generateKey(key, Supla::ConfigTag::HvacCfgTag);
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&config),
                     sizeof(TChannelConfig_HVAC))) {
      SUPLA_LOG_INFO("HVAC[%d]: config saved successfully", getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("HVAC[%d]: failed to save config", getChannelNumber());
    }

    if (channelConfigChangedOffline) {
      cfg->setChannelConfigChangeFlag(getChannelNumber());
    } else {
      cfg->clearChannelConfigChangeFlag(getChannelNumber());
    }

    cfg->saveWithDelay(5000);
  }
  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->notifyConfigChange(getChannelNumber());
  }
}

void HvacBase::saveWeeklySchedule() {
  auto cfg = Supla::Storage::ConfigInstance();
  if (cfg) {
    char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
    // Weekly schedule configuration
    generateKey(key, "hvac_weekly");
    if (cfg->setBlob(key,
                     reinterpret_cast<char *>(&weeklySchedule),
                     sizeof(TChannelConfig_WeeklySchedule))) {
      SUPLA_LOG_INFO("HVAC[%d]: weekly schedule saved successfully",
                     getChannelNumber());
    } else {
      SUPLA_LOG_WARNING("HVAC[%d]: failed to save weekly schedule",
                     getChannelNumber());
    }

    // for standard thermosat function, save also alternative schedule
    if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
      generateKey(key, "hvac_aweekly");
      if (cfg->setBlob(key,
                       reinterpret_cast<char *>(&altWeeklySchedule),
                       sizeof(TChannelConfig_WeeklySchedule))) {
        SUPLA_LOG_INFO("HVAC[%d]: alt weekly schedule saved successfully",
                       getChannelNumber());
      } else {
        SUPLA_LOG_WARNING("HVAC[%d]: failed to save alt weekly schedule",
                       getChannelNumber());
      }
    }

    generateKey(key, "weekly_chng");
    if (weeklyScheduleChangedOffline) {
      cfg->setUInt8(key, 1);
    } else {
      cfg->setUInt8(key, 0);
    }
    cfg->saveWithDelay(5000);
  }
}

void HvacBase::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  if (result == nullptr) {
    return;
  }

  bool success = (result->Result == SUPLA_CONFIG_RESULT_TRUE);
  (void)(success);

  switch (result->ConfigType) {
    case SUPLA_CONFIG_TYPE_DEFAULT: {
      SUPLA_LOG_INFO("HVAC[%d]: set channel config %s (%d)",
                     getChannelNumber(),
                     success ? "succeeded" : "failed",
                     result->Result);
      clearChannelConfigChangedFlag();
      break;
    }
    case SUPLA_CONFIG_TYPE_WEEKLY_SCHEDULE: {
      SUPLA_LOG_INFO("HVAC[%d]: set weekly schedule config %s (%d)",
                     getChannelNumber(),
                     success ? "succeeded" : "failed",
                     result->Result);
      clearWeeklyScheduleChangedFlag();
      break;
    }
    case SUPLA_CONFIG_TYPE_ALT_WEEKLY_SCHEDULE: {
      SUPLA_LOG_INFO("HVAC[%d]: set alt weekly schedule config %s (%d)",
                     getChannelNumber(),
                     success ? "succeeded" : "failed",
                     result->Result);
      clearWeeklyScheduleChangedFlag();
      break;
    }
    default:
      break;
  }
}

void HvacBase::clearChannelConfigChangedFlag() {
  if (channelConfigChangedOffline) {
    channelConfigChangedOffline = 0;
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      cfg->clearChannelConfigChangeFlag(getChannelNumber());
      cfg->saveWithDelay(1000);
    }
  }
}

void HvacBase::clearWeeklyScheduleChangedFlag() {
  if (weeklyScheduleChangedOffline) {
    weeklyScheduleChangedOffline = 0;
    auto cfg = Supla::Storage::ConfigInstance();
    if (cfg) {
      char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
      generateKey(key, "weekly_chng");
      cfg->setUInt8(key, 0);
      cfg->saveWithDelay(1000);
    }
  }
}

void HvacBase::debugPrintProgram(const TWeeklyScheduleProgram *program,
                                 int id) {
  SUPLA_LOG_DEBUG("HVAC Program[%d], mode %d, tHeat %d, tCool %d",
                  id,
                  program->Mode,
                  program->SetpointTemperatureHeat,
                  program->SetpointTemperatureCool);
}

bool HvacBase::isWeeklyScheduleValid(TChannelConfig_WeeklySchedule *newSchedule,
                                     bool isAltWeeklySchedule) const {
  bool programIsUsed[SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE] = {};

  // check if programs are valid
  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE; i++) {
    debugPrintProgram(&(newSchedule->Program[i]), i);
    if (!isProgramValid(newSchedule->Program[i], isAltWeeklySchedule)) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: invalid program %d",
          getChannelNumber(),
          i);
      return false;
    }
    if (newSchedule->Program[i].Mode != SUPLA_HVAC_MODE_NOT_SET) {
      programIsUsed[i] = true;
    }
  }

  // check if only used programs are configured in the schedule
  for (int i = 0; i < SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE; i++) {
    int programId = getWeeklyScheduleProgramId(newSchedule, i);
    if (programId != 0 && !programIsUsed[programId - 1]) {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: weekly schedule validation failed: not configured program "
          "used in schedule %d",
          getChannelNumber(),
          i);
      return false;
    }
  }

  return true;
}

int HvacBase::getWeeklyScheduleProgramId(
    const TChannelConfig_WeeklySchedule *schedule, int index) const {
  if (schedule == nullptr) {
    // use main weekly schedule
    schedule = &weeklySchedule;
  }
  if (index < 0 || index > SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE) {
    return 0;
  }

  return (schedule->Quarters[index / 2] >> (index % 2 * 4)) & 0xF;
}

int HvacBase::calculateIndex(enum DayOfWeek dayOfWeek,
                             int hour,
                             int quarter) const {
  if (dayOfWeek < DayOfWeek_Sunday || dayOfWeek > DayOfWeek_Saturday) {
    return -1;
  }
  if (hour < 0 || hour > 23) {
    return -1;
  }
  if (quarter < 0 || quarter > 3) {
    return -1;
  }

  return (dayOfWeek * 24 + hour) * 4 + quarter;
}

bool HvacBase::isProgramValid(const TWeeklyScheduleProgram &program,
    bool isAltWeeklySchedule) const {
  if (program.Mode == SUPLA_HVAC_MODE_NOT_SET) {
    return true;
  }

  if (program.Mode != SUPLA_HVAC_MODE_COOL
      && program.Mode != SUPLA_HVAC_MODE_HEAT
      && program.Mode != SUPLA_HVAC_MODE_HEAT_COOL) {
    return false;
  }

  auto channelFunction = channel.getDefaultFunction();
//  SUPLA_LOG_DEBUG("HVAC: channel function %d", channelFunction);
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (program.Mode == SUPLA_HVAC_MODE_HEAT) {
      if (isAltWeeklySchedule) {
        return false;
      }
      if (!isHeatingAndCoolingSupported()) {
        return false;
      }
    } else if (program.Mode == SUPLA_HVAC_MODE_COOL) {
      if (!isAltWeeklySchedule) {
        return false;
      }
      if (!isHeatingAndCoolingSupported()) {
        return false;
      }
    } else if (program.Mode != SUPLA_HVAC_MODE_NOT_SET &&
               program.Mode != SUPLA_HVAC_MODE_OFF) {
      return false;
    }
  } else if (!isModeSupported(program.Mode)) {
    return false;
  }

  // check tempreatures
  switch (program.Mode) {
    case SUPLA_HVAC_MODE_HEAT: {
      return isTemperatureInRoomConstrain(program.SetpointTemperatureHeat);
    }
    case SUPLA_HVAC_MODE_COOL: {
      return isTemperatureInRoomConstrain(program.SetpointTemperatureCool);
    }
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      return isTemperatureInHeatCoolConstrain(program.SetpointTemperatureHeat,
                                          program.SetpointTemperatureCool);
    }
    default: {
      return false;
    }
  }

  return true;
}

bool HvacBase::isModeSupported(int mode) const {
  bool heatSupported = false;
  bool coolSupported = false;
  bool heatCoolSupported = false;
  bool drySupported = false;
  bool fanSupported = isFanSupported();
  bool onOffSupported = true;

  switch (channel.getDefaultFunction()) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
      heatSupported = isHeatingSubfunction();
      coolSupported = isCoolingSubfunction();
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      heatSupported = true;
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      heatCoolSupported = true;
      heatSupported = true;
      coolSupported = true;
      drySupported = isDrySupported();
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DRYER: {
      drySupported = true;
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_FAN: {
      fanSupported = true;
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL: {
      heatSupported = true;
    }
  }

  switch (mode) {
    case SUPLA_HVAC_MODE_HEAT: {
      return heatSupported;
    }
    case SUPLA_HVAC_MODE_COOL: {
      return coolSupported;
    }
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      return heatCoolSupported;
    }
    case SUPLA_HVAC_MODE_FAN_ONLY: {
      return fanSupported;
    }
    case SUPLA_HVAC_MODE_DRY: {
      return drySupported;
    }
    case SUPLA_HVAC_MODE_CMD_TURN_ON:
    case SUPLA_HVAC_MODE_OFF: {
      return onOffSupported;
    }
    case SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE: {
      return isWeeklyScheduleConfigured;
    }
  }

  return false;
}

bool HvacBase::setWeeklySchedule(int index,
                                 int programId,
                                 bool isAltWeeklySchedule) {
  if (index < 0 || index >= SUPLA_WEEKLY_SCHEDULE_VALUES_SIZE) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid index %d", getChannelNumber(), index);
    return false;
  }
  if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d]: invalid programId %d", getChannelNumber(), programId);
    return false;
  }

  auto weeklySchedulePtr =
      (isAltWeeklySchedule ? &altWeeklySchedule : &weeklySchedule);

  if (programId > 0 &&
      (weeklySchedulePtr->Program[programId - 1].Mode ==
           SUPLA_HVAC_MODE_NOT_SET ||
       weeklySchedulePtr->Program[programId - 1].Mode > SUPLA_HVAC_MODE_DRY)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: invalid mode %d for programId %d",
                    getChannelNumber(),
                    weeklySchedulePtr->Program[programId - 1].Mode,
                    programId);
    return false;
  }

  if (index % 2) {
    weeklySchedulePtr->Quarters[index / 2] =
        (weeklySchedulePtr->Quarters[index / 2] & 0x0F) | (programId << 4);
  } else {
    weeklySchedulePtr->Quarters[index / 2] =
        (weeklySchedulePtr->Quarters[index / 2] & 0xF0) | programId;
  }

  if (initDone) {
    weeklyScheduleChangedOffline = 1;
    isWeeklyScheduleConfigured = true;
    saveWeeklySchedule();
  }

  isWeeklyScheduleConfigured = true;
  return true;
}

bool HvacBase::setWeeklySchedule(enum DayOfWeek dayOfWeek,
                                 int hour,
                                 int quarter,
                                 int programId,
                                 bool isAltWeeklySchedule) {
  return setWeeklySchedule(
      calculateIndex(dayOfWeek, hour, quarter), programId, isAltWeeklySchedule);
}

TWeeklyScheduleProgram HvacBase::getProgramAt(int quarterIndex) const {
  int programId = 1;

  auto weeklySchedulePtr = &weeklySchedule;
  if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
      weeklySchedulePtr = &altWeeklySchedule;
    }
  }

  if (quarterIndex >= 0) {
    programId = getWeeklyScheduleProgramId(weeklySchedulePtr, quarterIndex);
  }

  TWeeklyScheduleProgram program = {};
  program.SetpointTemperatureCool = INT16_MIN;
  program.SetpointTemperatureHeat = INT16_MIN;

  if (programId == 0) {
    program.Mode = SUPLA_HVAC_MODE_OFF;
    return program;
  }
  if (programId < 0 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    // return "NOT_SET", which will be considered as an error
    return program;
  }

  return weeklySchedulePtr->Program[programId - 1];
}

int HvacBase::getCurrentQuarter() const {
  int quarterIndex = -1;

  if (Supla::Clock::IsReady()) {
    quarterIndex = calculateIndex(Supla::Clock::GetHvacDayOfWeek(),
        Supla::Clock::GetHour(), Supla::Clock::GetQuarter());
  }
  return quarterIndex;
}

TWeeklyScheduleProgram HvacBase::getCurrentProgram() const {
  return getProgramAt(getCurrentQuarter());
}

int HvacBase::getCurrentProgramId() const {
  int quarterIndex = getCurrentQuarter();

  int programId = 1;

  auto weeklySchedulePtr = &weeklySchedule;
  if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL) {
      weeklySchedulePtr = &altWeeklySchedule;
    }
  }

  if (quarterIndex >= 0) {
    programId = getWeeklyScheduleProgramId(weeklySchedulePtr, quarterIndex);
  }

  return programId;
}

bool HvacBase::setProgram(int programId,
                          unsigned char mode,
                          _supla_int16_t tHeat,
                          _supla_int16_t tCool,
                          bool isAltWeeklySchedule) {
  SUPLA_LOG_DEBUG("HVAC[%d]: set%sProgram(%d, %d, %d, %d, %d)",
                  channel.getChannelNumber(),
                  isAltWeeklySchedule ? "Alt" : "Main",
                  programId,
                  mode,
                  tHeat,
                  tCool);

  TWeeklyScheduleProgram program = {mode, {tHeat}, {tCool}};

  if (!isProgramValid(program, isAltWeeklySchedule)) {
    return false;
  }

  auto schedule = (isAltWeeklySchedule ? &altWeeklySchedule : &weeklySchedule);

  schedule->Program[programId - 1].Mode = mode;
  schedule->Program[programId - 1].SetpointTemperatureHeat = tHeat;
  schedule->Program[programId - 1].SetpointTemperatureCool = tCool;

  if (initDone) {
    weeklyScheduleChangedOffline = 1;
    isWeeklyScheduleConfigured = true;
    saveWeeklySchedule();
  }
  return true;
}

TWeeklyScheduleProgram HvacBase::getProgramById(
    int programId, bool isAltWeeklySchedule) const {
  if (programId < 1 || programId > SUPLA_WEEKLY_SCHEDULE_PROGRAMS_MAX_SIZE) {
    return {};
  }

  if (isAltWeeklySchedule) {
    return altWeeklySchedule.Program[programId - 1];
  } else {
    return weeklySchedule.Program[programId - 1];
  }
}

_supla_int16_t HvacBase::getPrimaryTemp() {
  return getTemperature(config.MainThermometerChannelNo);
}

_supla_int16_t HvacBase::getSecondaryTemp() {
  auto type = getAuxThermometerType();
  if (type != SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET &&
      type != SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED) {
    return getTemperature(getAuxThermometerChannelNo());
  }

  return INT16_MIN;
}

_supla_int16_t HvacBase::getTemperature(int channelNo) {
  if (channelNo >= 0 && channelNo != getChannelNumber()) {
    auto el = Supla::Element::getElementByChannelNumber(channelNo);

    if (!el) {
      return INT16_MIN;
    }

    double temp = el->getChannel()->getLastTemperature();
    if (temp <= TEMPERATURE_NOT_AVAILABLE) {
      return INT16_MIN;
    }
    temp *= 100;
    if (temp > INT16_MAX) {
      return INT16_MAX;
    }
    if (temp <= INT16_MIN) {
      return INT16_MIN + 1;
    }
    return temp;
  }
  return INT16_MIN;
}

void HvacBase::setOutput(int value, bool force) {
  if (primaryOutput == nullptr) {
    return;
  }

  if (lastValue == value) {
    return;
  }

  if (lastValue < -100) {
    lastValue = 0;
  }

  bool stateChanged = false;
  // make sure that min on/off time configuration is respected
  if (lastValue > 0 && value <= 0) {
    if (!force && millis() - lastOutputStateChangeTimestampMs <
        config.MinOnTimeS * 1000) {
      return;
    }
    // when output should change from heating to cooling, we add off step
    // between
    value = 0;
    stateChanged = true;
  }

  if (lastValue == 0 && value != 0) {
    if (!force && millis() - lastOutputStateChangeTimestampMs <
        config.MinOffTimeS * 1000) {
      return;
    }
    stateChanged = true;
  }

  if (lastValue < 0 && value >= 0) {
    if (!force && millis() - lastOutputStateChangeTimestampMs <
        config.MinOnTimeS * 1000) {
      return;
    }
    // when output should change from cooling to heating, we add off step
    // between
    value = 0;
    stateChanged = true;
  }

  if (stateChanged) {
    lastOutputStateChangeTimestampMs = millis();
  }
  lastValue = value;

  auto channelFunction = channel.getDefaultFunction();

  if (isHeatingSubfunction() ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    if (secondaryOutput) {
      secondaryOutput->setOutputValue(0);
    }

    if (value <= 0) {
      primaryOutput->setOutputValue(0);
    } else {
      if (primaryOutput->isOnOffOnly()) {
        value = 1;
      }
      primaryOutput->setOutputValue(value);
    }
  } else if (isCoolingSubfunction()) {
    auto output = primaryOutput;
    if (secondaryOutput) {
      output = secondaryOutput;
      primaryOutput->setOutputValue(0);
    }

    if (value >= 0) {
      output->setOutputValue(0);
    } else {
      if (primaryOutput->isOnOffOnly()) {
        value = -1;
      } else {
        value = -value;
      }
      output->setOutputValue(value);
    }
  } else if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
    if (secondaryOutput == nullptr) {
      return;
    }

    if (value == 0) {
      primaryOutput->setOutputValue(0);
      secondaryOutput->setOutputValue(0);
    } else if (value >= 1) {
      secondaryOutput->setOutputValue(0);

      if (primaryOutput->isOnOffOnly()) {
        value = 1;
      }
      primaryOutput->setOutputValue(value);
    } else if (value <= -1) {
      primaryOutput->setOutputValue(0);

      if (secondaryOutput->isOnOffOnly()) {
        value = -1;
      } else {
        value = -value;
      }

      secondaryOutput->setOutputValue(value);
    }
  }
  updateChannelState();
}

void HvacBase::setTargetMode(int mode, bool keepScheduleOn) {
//  SUPLA_LOG_DEBUG("HVAC[%d]: set target mode %s, keepScheduleOn %d",
//                  getChannelNumber(),
//                  channel.getHvacModeCstr(mode),
//                  keepScheduleOn);
  channel.setHvacFlagCountdownTimer(false);
  if (channel.getHvacMode() == mode) {
    if (!(!keepScheduleOn && channel.isHvacFlagWeeklySchedule())) {
      return;
    }
  }

  if (!isOutputControlledInternally() &&
      channel.isHvacFlagForcedOffBySensor() &&
      mode != SUPLA_HVAC_MODE_OFF) {
    return;
  }

  if (!keepScheduleOn && mode != SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE) {
    lastProgramManualOverride = -1;
  }

  SUPLA_LOG_INFO("HVAC[%d]: set target mode %s requested (%d)",
                 getChannelNumber(),
                 channel.getHvacModeCstr(mode), mode);

  if (isModeSupported(mode)) {
    if (mode == SUPLA_HVAC_MODE_OFF) {
      storeLastWorkingMode();
      if (!keepScheduleOn && channel.isHvacFlagWeeklySchedule()) {
        channel.setHvacFlagWeeklySchedule(false);
        lastWorkingMode.Mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
      }
      channel.setHvacMode(mode);
      setOutput(0, false);
    } else if (mode == SUPLA_HVAC_MODE_CMD_TURN_ON) {
      if (channel.getHvacMode() == SUPLA_HVAC_MODE_OFF ||
          !isModeSupported(channel.getHvacMode())) {
        turnOn();
      }
      keepScheduleOn = true;
    } else if (mode == SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE) {
      keepScheduleOn = true;
      if (!turnOnWeeklySchedlue()) {
        return;
      }
    } else {
      channel.setHvacMode(mode);
    }

    if (!keepScheduleOn) {
      channel.setHvacFlagWeeklySchedule(false);
    }

    fixTemperatureSetpoints();

    lastConfigChangeTimestampMs = millis();
  }
  SUPLA_LOG_INFO("HVAC[%d]: current mode %s",
                 getChannelNumber(),
                 channel.getHvacModeCstr());
}

bool HvacBase::checkAntifreezeProtection(_supla_int16_t t) {
  // antifreeze can be used when it is enabled, and when current function and
  // subfunction allows heating
  if (isAntiFreezeAndHeatProtectionEnabled() &&
      isModeSupported(SUPLA_HVAC_MODE_HEAT)) {
    auto tFreeze = getTemperatureFreezeProtection();
    if (!isSensorTempValid(tFreeze)) {
      return false;
    }

    auto outputValue = evaluateHeatOutputValue(t, tFreeze);
    if (outputValue > 0) {
      setOutput(outputValue, false);
      return true;
    }
  }
  return false;
}

bool HvacBase::checkOverheatProtection(_supla_int16_t t) {
  // overheat can be used when it is enabled, and when current function and
  // subfunction allows cooling
  if (isAntiFreezeAndHeatProtectionEnabled() &&
      isModeSupported(SUPLA_HVAC_MODE_COOL)) {
    auto tOverheat = getTemperatureHeatProtection();
    if (!isSensorTempValid(tOverheat)) {
      return false;
    }

    auto outputValue = evaluateCoolOutputValue(t, tOverheat);
    if (outputValue < 0) {
      setOutput(outputValue, false);
      return true;
    }
  }
  return false;
}

bool HvacBase::isAuxProtectionEnabled() const {
  if (!isAuxMinMaxSetpointEnabled()) {
    return false;
  }
  auto type = getAuxThermometerType();
  if (type == SUPLA_HVAC_AUX_THERMOMETER_TYPE_NOT_SET ||
      type == SUPLA_HVAC_AUX_THERMOMETER_TYPE_DISABLED) {
    return false;
  }
  auto tAuxMin = getTemperatureAuxMinSetpoint();
  auto tAuxMax = getTemperatureAuxMaxSetpoint();
  if (!isSensorTempValid(tAuxMin) && !isSensorTempValid(tAuxMax)) {
    return false;
  }

  return true;
}

bool HvacBase::checkAuxProtection(_supla_int16_t t) {
  if (!isAuxProtectionEnabled()) {
    return false;
  }

  if (channel.getHvacMode() == SUPLA_HVAC_MODE_OFF ||
      channel.getHvacMode() == SUPLA_HVAC_MODE_NOT_SET) {
    return false;
  }

  auto tAuxMin = getTemperatureAuxMinSetpoint();
  auto tAuxMax = getTemperatureAuxMaxSetpoint();
  if (isSensorTempValid(tAuxMin)) {
    auto outputValue = evaluateHeatOutputValue(t, tAuxMin);
    if (outputValue > 0) {
      setOutput(outputValue, false);
      return true;
    }
  }

  if (isSensorTempValid(tAuxMax)) {
    auto outputValue = evaluateCoolOutputValue(t, tAuxMax);
    if (outputValue < 0) {
      setOutput(outputValue, false);
      return true;
    }
  }
  return false;
}

void HvacBase::copyFixedChannelConfigTo(HvacBase *hvac) const {
  if (hvac == nullptr) {
    return;
  }
  hvac->addAvailableAlgorithm(config.AvailableAlgorithms);
  hvac->setTemperatureRoomMin(getTemperatureRoomMin());
  hvac->setTemperatureRoomMax(getTemperatureRoomMax());
  hvac->setTemperatureAuxMin(getTemperatureAuxMin());
  hvac->setTemperatureAuxMax(getTemperatureAuxMax());
  hvac->setTemperatureHisteresisMin(getTemperatureHisteresisMin());
  hvac->setTemperatureHisteresisMax(getTemperatureHisteresisMax());
  hvac->setTemperatureHeatCoolOffsetMin(getTemperatureHeatCoolOffsetMin());
  hvac->setTemperatureHeatCoolOffsetMax(getTemperatureHeatCoolOffsetMax());
}

void HvacBase::copyFullChannelConfigTo(TChannelConfig_HVAC *hvac) const {
  if (hvac == nullptr) {
    return;
  }

  memcpy(hvac, &config, sizeof(TChannelConfig_HVAC));
}

bool HvacBase::applyNewRuntimeSettings(int mode, int32_t durationSec) {
  return applyNewRuntimeSettings(mode,
                                 INT16_MIN,  // getTemperatureSetpointHeat(),
                                 INT16_MIN,  // getTemperatureSetpointCool(),
                                 durationSec);
}

bool HvacBase::applyNewRuntimeSettings(int mode,
                                       int16_t tHeat,
                                       int16_t tCool,
                                       int32_t durationSec) {
  SUPLA_LOG_DEBUG(
      "HVAC[%d]: applyNewRuntimeSettings mode=%s tHeat=%d tCool=%d, "
      "durationSec=%d",
      getChannelNumber(),
      channel.getHvacModeCstr(mode),
      tHeat,
      tCool,
      durationSec);
  if (mode == SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE) {
    lastProgramManualOverride = -1;
  }

  if (mode == SUPLA_HVAC_MODE_NOT_SET) {
    if (isWeeklyScheduleEnabled() &&
        !isTemperatureSetpointChangeSwitchesToManualMode()) {
      lastProgramManualOverride = getCurrentProgramId();
      mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
    } else {
      mode = SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL;
    }
  }

  if (mode == SUPLA_HVAC_MODE_CMD_SWITCH_TO_MANUAL) {
    mode = lastManualMode;
    if (mode == SUPLA_HVAC_MODE_NOT_SET) {
      mode = getDefaultManualMode();
      if (mode == SUPLA_HVAC_MODE_HEAT_COOL) {
        if (tHeat == INT16_MIN && tCool > INT16_MIN) {
          mode = SUPLA_HVAC_MODE_COOL;
        } else if (tHeat > INT16_MIN && tCool == INT16_MIN) {
          mode = SUPLA_HVAC_MODE_HEAT;
        }
      }
    }
  }

  if (!isModeSupported(mode)) {
    SUPLA_LOG_WARNING("HVAC[%d]: applyNewRuntimeSettings mode=%s not supported",
                      getChannelNumber(),
                      channel.getHvacModeCstr(mode));
    return false;
  }

  if (durationSec > 0) {
    if (Supla::Clock::IsReady()) {
      time_t now = Supla::Clock::GetTimeStamp();
      countdownTimerEnds = now + durationSec;
      channel.setHvacFlagClockError(false);
      if (countdownTimerEnds <= now) {
        SUPLA_LOG_WARNING(
            "HVAC[%d]: applyNewRuntimeSettings countdown timer ends in the "
            "past",
            getChannelNumber());
        return false;
      }
      if (!isCountdownEnabled()) {
        storeLastWorkingMode();
      }
    } else {
      SUPLA_LOG_WARNING("HVAC[%d]: applyNewRuntimeSettings clock is not ready!",
                        getChannelNumber());
      channel.setHvacFlagClockError(true);
      return false;
    }
  } else {
    channel.setHvacFlagCountdownTimer(false);
    countdownTimerEnds = 1;
  }

  setTargetMode(mode, false);

  if ((mode != SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE &&
        !isWeeklyScheduleEnabled()) ||
      isWeelkySchedulManualOverrideMode()) {
    setSetpointTemperaturesForCurrentMode(tHeat, tCool);
  }

  if (durationSec > 0) {
    channel.setHvacFlagCountdownTimer(true);
  }

  if (!channel.isHvacFlagWeeklySchedule() && mode != SUPLA_HVAC_MODE_OFF &&
      !channel.isHvacFlagCountdownTimer()) {
    // in manual mode we store last manual temperatures
    if (mode == SUPLA_HVAC_MODE_HEAT_COOL || mode == SUPLA_HVAC_MODE_COOL ||
        mode == SUPLA_HVAC_MODE_HEAT) {
      lastManualMode = mode;
    }
    if (tHeat > INT16_MIN) {
      lastManualSetpointHeat = tHeat;
    }
    if (tCool > INT16_MIN) {
      lastManualSetpointCool = tCool;
    }
  }

  return true;
}

int32_t HvacBase::handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue) {
  if (isMasterThermostatSet()) {
    // ignore new value from server
    SUPLA_LOG_DEBUG("HVAC[%d]: ignore new value from server (master is set)",
                    getChannelNumber());
    return 0;
  }
  auto hvacValue = reinterpret_cast<THVACValue *>(newValue->value);
  SUPLA_LOG_DEBUG(
      "HVAC[%d]: new value from server: mode=%s tHeat=%d tCool=%d, "
      "durationSec=%d",
      getChannelNumber(),
      channel.getHvacModeCstr(hvacValue->Mode),
      hvacValue->SetpointTemperatureHeat,
      hvacValue->SetpointTemperatureCool,
      newValue->DurationSec);

  int tHeat = INT16_MIN;  // hvacValue->SetpointTemperatureHeat;
  int tCool = INT16_MIN;  // hvacValue->SetpointTemperatureCool;

  if (hvacValue->Mode != SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE) {
    if (Supla::Channel::isHvacFlagSetpointTemperatureCoolSet(hvacValue)) {
      if (isTemperatureInRoomConstrain(hvacValue->SetpointTemperatureCool)) {
        tCool = hvacValue->SetpointTemperatureCool;
      } else {
        tCool = lastManualSetpointCool;
      }
    } else {
      tCool = lastManualSetpointCool;  // getTemperatureSetpointCool();
    }

    if (Supla::Channel::isHvacFlagSetpointTemperatureHeatSet(hvacValue)) {
      if (isTemperatureInRoomConstrain(hvacValue->SetpointTemperatureHeat)) {
        tHeat = hvacValue->SetpointTemperatureHeat;
      } else {
        tHeat = lastManualSetpointHeat;
      }
    } else {
      tHeat = lastManualSetpointHeat;  // getTemperatureSetpointHeat();
    }
  }

  if (applyNewRuntimeSettings(
      hvacValue->Mode, tHeat, tCool, newValue->DurationSec)) {
    // clear flag, so iterateAlways method will apply new config instantly
    // instead of waiting few seconds
    lastConfigChangeTimestampMs = 0;
    return 1;
  }

  return 0;
}

void HvacBase::setTemperatureSetpointHeat(int tHeat) {
  if (tHeat == INT16_MIN) {
    return;
  }
  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  if (tHeat < tMin) {
    tHeat = tMin;
  }
  if (tHeat > tMax) {
    tHeat = tMax;
  }

  channel.setHvacSetpointTemperatureHeat(tHeat);
  lastConfigChangeTimestampMs = millis();
}

void HvacBase::setTemperatureSetpointCool(int tCool) {
  if (tCool == INT16_MIN) {
    return;
  }
  auto tMin =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MIN);
  auto tMax =
      getTemperatureFromStruct(&config.Temperatures, TEMPERATURE_ROOM_MAX);

  if (tCool < tMin) {
    tCool = tMin;
  }
  if (tCool > tMax) {
    tCool = tMax;
  }

  channel.setHvacSetpointTemperatureCool(tCool);
  lastConfigChangeTimestampMs = millis();
}

void HvacBase::clearTemperatureSetpointHeat() {
  channel.clearHvacSetpointTemperatureHeat();
  lastConfigChangeTimestampMs = millis();
}

void HvacBase::clearTemperatureSetpointCool() {
  channel.clearHvacSetpointTemperatureCool();
  lastConfigChangeTimestampMs = millis();
}

int HvacBase::getTemperatureSetpointHeat() {
  if (channel.isHvacFlagSetpointTemperatureHeatSet()) {
    return channel.getHvacSetpointTemperatureHeat();
  }
  return INT16_MIN;
}

int HvacBase::getTemperatureSetpointCool() {
  if (channel.isHvacFlagSetpointTemperatureCoolSet()) {
    return channel.getHvacSetpointTemperatureCool();
  }
  return INT16_MIN;
}

int HvacBase::getDefaultManualMode() {
    switch (channel.getDefaultFunction()) {
      case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
        if (isHeatingSubfunction()) {
          return SUPLA_HVAC_MODE_HEAT;
        } else if (isCoolingSubfunction()) {
          return SUPLA_HVAC_MODE_COOL;
        }
        break;
      }
      case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
        return SUPLA_HVAC_MODE_HEAT;
      }
      case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
        return SUPLA_HVAC_MODE_HEAT_COOL;
      }
    }
  return SUPLA_HVAC_MODE_OFF;
}

void HvacBase::turnOn() {
  if (channel.isHvacFlagWeeklySchedule() ||
      lastWorkingMode.Mode == SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE) {
    turnOnWeeklySchedlue();
    return;
  }

  uint8_t mode = SUPLA_HVAC_MODE_OFF;
  if (!isModeSupported(lastWorkingMode.Mode)) {
    lastWorkingMode.Mode = SUPLA_HVAC_MODE_NOT_SET;
  }

  if (lastWorkingMode.Mode != SUPLA_HVAC_MODE_NOT_SET) {
    mode = lastWorkingMode.Mode;
  } else {
    mode = getDefaultManualMode();
  }
  if (Supla::Channel::isHvacFlagSetpointTemperatureHeatSet(&lastWorkingMode)) {
    setTemperatureSetpointHeat(lastWorkingMode.SetpointTemperatureHeat);
  }
  if (Supla::Channel::isHvacFlagSetpointTemperatureCoolSet(&lastWorkingMode)) {
    setTemperatureSetpointCool(lastWorkingMode.SetpointTemperatureCool);
  }
  setTargetMode(mode, false);
}

bool HvacBase::turnOnWeeklySchedlue() {
  if (!isWeeklyScheduleConfigured) {
    return false;
  }

  channel.setHvacFlagWeeklySchedule(true);
  return processWeeklySchedule();
}

bool HvacBase::processWeeklySchedule() {
  if (!channel.isHvacFlagWeeklySchedule()) {
    SUPLA_LOG_WARNING(
        "HVAC[%d]: processs weekly schedule failed - it is not enabled",
        getChannelNumber());
    return false;
  }

  if (!Supla::Clock::IsReady()) {
    if (!channel.isHvacFlagClockError()) {
      // print only on first error
      SUPLA_LOG_WARNING(
          "HVAC[%d]: processs weekly schedule failed - clock is not ready",
          getChannelNumber());
    }
    channel.setHvacFlagClockError(true);
  } else {
    channel.setHvacFlagClockError(false);
  }

  TWeeklyScheduleProgram program = getCurrentProgram();
  if (program.Mode == SUPLA_HVAC_MODE_NOT_SET) {
    SUPLA_LOG_INFO(
        "HVAC[%d]: Invalid program mode. Disabling schedule.",
        getChannelNumber());
    setTargetMode(SUPLA_HVAC_MODE_OFF, false);
    return false;
  } else {
    if (isWeelkySchedulManualOverrideMode()) {
      int currentProgramId = getCurrentProgramId();
      if (currentProgramId != lastProgramManualOverride) {
        SUPLA_LOG_DEBUG("HVAC[%d]: leaving manual override mode",
                        getChannelNumber());
        lastProgramManualOverride = -1;
      } else {
        if (getMode() == SUPLA_HVAC_MODE_OFF) {
          int mode = lastManualMode;
          if (mode == 0) {
            mode = getDefaultManualMode();
          }
          SUPLA_LOG_DEBUG("HVAC[%d]: Manual override mode %d",
                          getChannelNumber(),
                          mode);
          setTargetMode(mode, true);
        }
        if (!channel.isHvacFlagWeeklyScheduleTemporalOverride()) {
          // print only on first entry
          SUPLA_LOG_DEBUG("HVAC[%d]: Manual override mode", getChannelNumber());
        }
        channel.setHvacFlagWeeklyScheduleTemporalOverride(true);
        return true;
      }
    }
    channel.setHvacFlagWeeklyScheduleTemporalOverride(false);
    setTargetMode(program.Mode, true);
    int16_t tHeat = program.SetpointTemperatureHeat;
    int16_t tCool = program.SetpointTemperatureCool;
    if (program.Mode == SUPLA_HVAC_MODE_HEAT) {
      tCool = INT16_MIN;
    }
    if (program.Mode == SUPLA_HVAC_MODE_COOL) {
      tHeat = INT16_MIN;
    }
    setSetpointTemperaturesForCurrentMode(tHeat, tCool);
  }
  return true;
}

void HvacBase::setSetpointTemperaturesForCurrentMode(int16_t tHeat,
                                                     int16_t tCool) {
  if (!channel.isHvacFlagWeeklySchedule()) {
    if (tHeat == INT16_MIN) {
      tHeat = lastManualSetpointHeat;
    }
    if (tCool == INT16_MIN) {
      tCool = lastManualSetpointCool;
    }
  }

  setTemperatureSetpointHeat(tHeat);
  setTemperatureSetpointCool(tCool);
}

void HvacBase::addPrimaryOutput(Supla::Control::OutputInterface *output) {
  primaryOutput = output;

  if (primaryOutput == nullptr) {
    channel.setFuncList(0);
    return;
  }

  setHeatingAndCoolingSupported(true);

  if (secondaryOutput != nullptr) {
    setHeatCoolSupported(true);
  }
}

void HvacBase::addSecondaryOutput(Supla::Control::OutputInterface *output) {
  secondaryOutput = output;

  if (secondaryOutput == nullptr) {
    setHeatCoolSupported(false);
    return;
  }

  if (primaryOutput != nullptr) {
    setHeatCoolSupported(true);
  }
}

bool HvacBase::isSensorTempValid(_supla_int16_t temperature) const {
  return temperature > INT16_MIN;
}

bool HvacBase::checkThermometersStatusForCurrentMode(
    _supla_int16_t t1, _supla_int16_t t2) const {
  switch (channel.getHvacMode()) {
    case SUPLA_HVAC_MODE_HEAT:
    case SUPLA_HVAC_MODE_COOL:
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      if (!isSensorTempValid(t1)) {
        return false;
      }
    }
  }

  if (channel.getDefaultFunction() ==
      SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    if (!isSensorTempValid(t1) || !isSensorTempValid(t2)) {
      return false;
    }
  }

  if (isAuxProtectionEnabled() && !isSensorTempValid(t2)) {
    return false;
  }

  return true;
}

int HvacBase::evaluateHeatOutputValue(_supla_int16_t tMeasured,
                                  _supla_int16_t tTarget) {
  if (!isSensorTempValid(tMeasured)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: tMeasured not valid", getChannelNumber());
    channel.setHvacFlagThermometerError(true);
    return getOutputValueOnError();
  }
  channel.setHvacFlagThermometerError(false);
  if (!isSensorTempValid(tTarget)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: tTarget not valid", getChannelNumber());
    return getOutputValueOnError();
  }

  if (!isOutputControlledInternally()) {
    return 0;
  }

  initDefaultAlgorithm();
  if (getUsedAlgorithm() == SUPLA_HVAC_ALGORITHM_NOT_SET) {
    SUPLA_LOG_DEBUG("HVAC[%d]: algorithm not valid", getChannelNumber());
    return getOutputValueOnError();
  }

  int output = lastValue;

  auto histeresis = getTemperatureHisteresis();
  if (!isSensorTempValid(histeresis)) {
    histeresis = getTemperatureHisteresisMin();
  }
  if (!isSensorTempValid(histeresis)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: histeresis not valid", getChannelNumber());
    return getOutputValueOnError();
  }

  auto histeresisHeat = histeresis;
  auto histeresisOff = histeresis;

  switch (getUsedAlgorithm()) {
    case SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE: {
      histeresis >>= 1;
      histeresisHeat = histeresis;
      histeresisOff = histeresis;
      break;
    }
    case SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST: {
      histeresisOff = 0;
      break;
    }
  }
  // check if we should turn on heating
  if (lastValue <= 0) {
    if (tMeasured < tTarget - histeresisHeat) {
      output = 100;
    }
  }

  // check if we should turn off heating
  if (lastValue > 0) {
    if (tMeasured > tTarget + histeresisOff) {
      output = 0;
    }
  }

  return output;
}

int HvacBase::evaluateCoolOutputValue(_supla_int16_t tMeasured,
                                  _supla_int16_t tTarget) {
  if (!isSensorTempValid(tMeasured)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: tMeasured not valid", getChannelNumber());
    channel.setHvacFlagThermometerError(true);
    return getOutputValueOnError();
  }
  channel.setHvacFlagThermometerError(false);

  if (!isSensorTempValid(tTarget)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: tTarget not valid", getChannelNumber());
    return getOutputValueOnError();
  }

  if (!isOutputControlledInternally()) {
    return 0;
  }

  initDefaultAlgorithm();
  if (getUsedAlgorithm() == SUPLA_HVAC_ALGORITHM_NOT_SET) {
    SUPLA_LOG_DEBUG("HVAC[%d]: algorithm not valid", getChannelNumber());
    return getOutputValueOnError();
  }

  int output = lastValue;

  auto histeresis = getTemperatureHisteresis();
  if (!isSensorTempValid(histeresis)) {
    histeresis = getTemperatureHisteresisMin();
  }
  if (!isSensorTempValid(histeresis)) {
    SUPLA_LOG_DEBUG("HVAC[%d]: histeresis not valid", getChannelNumber());
    return getOutputValueOnError();
  }

  auto histeresisCool = histeresis;
  auto histeresisOff = histeresis;

  switch (getUsedAlgorithm()) {
    case SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE: {
      histeresis >>= 1;
      histeresisCool = histeresis;
      histeresisOff = histeresis;
      break;
    }
    case SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST: {
      histeresisOff = 0;
      break;
    }
  }
  // check if we should turn on cooling
  // -111 is a magic "not used" value... I'm sorry for that
  if (lastValue >= 0 || lastValue == -111) {
    if (tMeasured > tTarget + histeresisCool) {
      output = -100;
    }
  }

  // check if we should turn off cooling
  if (lastValue < 0) {
    if (tMeasured < tTarget - histeresisOff) {
      output = 0;
    }
  }

  return output;
}
void HvacBase::changeFunction(int32_t newFunction, bool changedLocally) {
  auto currentFunction = channel.getDefaultFunction();
  if (currentFunction == newFunction) {
    return;
  }

  if (newFunction == 0) {
    // for "none/disbled" function we keep internal setting and then we
    // initialize default configuration
    newFunction = channel.getDefaultFunction();
  }

  setAndSaveFunction(newFunction);
  lastConfigChangeTimestampMs = millis();

  memset(&lastWorkingMode, 0, sizeof(lastWorkingMode));
  lastManualMode = 0;
  lastManualSetpointCool = INT16_MIN;
  lastManualSetpointHeat = INT16_MIN;

  initDefaultConfig();

  channel.clearHvacState();

  initDefaultWeeklySchedule();

  if (changedLocally) {
    channelConfigChangedOffline = 1;
  }
  saveConfig();

  clearLastOutputValue();
  setOutput(0, true);
  lastIterateTimestampMs = 0;
}

void HvacBase::enableDifferentialFunctionSupport() {
  channel.addToFuncList(SUPLA_BIT_FUNC_HVAC_THERMOSTAT_DIFFERENTIAL);
}

bool HvacBase::isDifferentialFunctionSupported() const {
  return channel.getFuncList() & SUPLA_BIT_FUNC_HVAC_THERMOSTAT_DIFFERENTIAL;
}

void HvacBase::enableDomesticHotWaterFunctionSupport() {
  channel.addToFuncList(SUPLA_BIT_FUNC_HVAC_DOMESTIC_HOT_WATER);
}

bool HvacBase::isDomesticHotWaterFunctionSupported() const {
  return channel.getFuncList() & SUPLA_BIT_FUNC_HVAC_DOMESTIC_HOT_WATER;
}

void HvacBase::fixTemperatureSetpoints() {
  auto curTHeat = getTemperatureSetpointHeat();
  auto curTCool = getTemperatureSetpointCool();

  if (isModeSupported(SUPLA_HVAC_MODE_HEAT) &&
      !channel.isHvacFlagSetpointTemperatureHeatSet()) {
    channel.setHvacSetpointTemperatureHeat(SUPLA_HVAC_DEFAULT_TEMP_HEAT);
  }
  if (isModeSupported(SUPLA_HVAC_MODE_COOL) &&
      !channel.isHvacFlagSetpointTemperatureCoolSet()) {
    channel.setHvacSetpointTemperatureCool(SUPLA_HVAC_DEFAULT_TEMP_COOL);
  }

  switch (channel.getHvacMode()) {
    case SUPLA_HVAC_MODE_HEAT: {
      if (!isSensorTempValid(curTHeat)) {
        setTemperatureSetpointHeat(SUPLA_HVAC_DEFAULT_TEMP_HEAT);
      }
      break;
    }
    case SUPLA_HVAC_MODE_COOL: {
      if (!isSensorTempValid(curTCool)) {
        setTemperatureSetpointCool(SUPLA_HVAC_DEFAULT_TEMP_COOL);
      }
      break;
    }
    case SUPLA_HVAC_MODE_HEAT_COOL: {
      bool tHeatValid = isSensorTempValid(curTHeat);
      bool tCoolValid = isSensorTempValid(curTCool);

      if (tHeatValid && tCoolValid) {
        if (!isTemperatureInHeatCoolConstrain(curTHeat, curTCool)) {
          auto offsetMin = getTemperatureHeatCoolOffsetMin();
          auto offsetMax = getTemperatureHeatCoolOffsetMax();
          if (curTCool - curTHeat < offsetMin) {
            if (isTemperatureInRoomConstrain(curTHeat + offsetMin)) {
              setTemperatureSetpointCool(curTHeat + offsetMin);
            } else {
              setTemperatureSetpointHeat(curTCool - offsetMin);
            }
          }
          if (curTCool - curTHeat > offsetMax) {
            if (isTemperatureInRoomConstrain(curTHeat + offsetMax)) {
              setTemperatureSetpointCool(curTHeat + offsetMax);
            } else {
              setTemperatureSetpointHeat(curTCool - offsetMax);
            }
          }
        } else if (tHeatValid) {
          channel.setHvacSetpointTemperatureHeat(curTHeat);
        } else if (tCoolValid) {
          channel.setHvacSetpointTemperatureCool(curTHeat);
        }
      }
      break;
    }
    default: {
      // other modes doesn't require temperature setpoints fix
      return;
    }
  }
}

void HvacBase::clearLastOutputValue() {
  lastValue = -111;
  lastOutputStateChangeTimestampMs = 0;
}

void HvacBase::storeLastWorkingMode() {
  auto hvacValue = channel.getValueHvac();
  if (hvacValue) {
    memcpy(&lastWorkingMode, hvacValue, sizeof(lastWorkingMode));
    if (channel.isHvacFlagWeeklySchedule()) {
      lastWorkingMode.Mode = SUPLA_HVAC_MODE_CMD_WEEKLY_SCHEDULE;
    }
  }
}

_supla_int16_t HvacBase::getLastTemperature() {
  return lastTemperature;
}

_supla_int_t HvacBase::getChannelFunction() {
  return channel.getDefaultFunction();
}

int HvacBase::getMode() {
  return channel.getHvacMode();
}

bool HvacBase::isWeeklyScheduleEnabled() {
  return channel.isHvacFlagWeeklySchedule();
}

bool HvacBase::isCountdownEnabled() {
  return channel.isHvacFlagCountdownTimer();
}

bool HvacBase::isThermostatDisabled() {
  return getMode() == SUPLA_HVAC_MODE_OFF;
}

bool HvacBase::isManualModeEnabled() {
  return !isThermostatDisabled() && !isWeeklyScheduleEnabled() &&
         !isCountdownEnabled();
}

void HvacBase::debugPrintConfigStruct(const TChannelConfig_HVAC *config,
                                      int id) {
  SUPLA_LOG_DEBUG("HVAC[%d]:", id);
  SUPLA_LOG_DEBUG("  Main: %d", config->MainThermometerChannelNo);
  SUPLA_LOG_DEBUG("  Aux: %d%s (type: %d)",
                  config->AuxThermometerChannelNo,
                  (config->AuxThermometerChannelNo == id ? " (disabled)" : ""),
                  config->AuxThermometerType);
  SUPLA_LOG_DEBUG("  AntiFreezeAndOverheatProtectionEnabled: %d",
                  config->AntiFreezeAndOverheatProtectionEnabled);
  SUPLA_LOG_DEBUG("  Sensor: %d%s", config->BinarySensorChannelNo,
                  (config->BinarySensorChannelNo == id ? " (disabled)" : ""));
  SUPLA_LOG_DEBUG("  Algorithms: %d (used: %d)",
                  config->AvailableAlgorithms,
                  config->UsedAlgorithm);
  SUPLA_LOG_DEBUG("  Min On/off TimeS: on: %d off: %d",
                  config->MinOnTimeS,
                  config->MinOffTimeS);
  SUPLA_LOG_DEBUG("  OutputValueOnError: %d", config->OutputValueOnError);
  SUPLA_LOG_DEBUG("  Subfunction: %s (%d)",
                  (config->Subfunction == 1) ? "HEAT" :
                  (config->Subfunction == 2) ? "COOL" : "N/A",
                  config->Subfunction);
  SUPLA_LOG_DEBUG("  Setpoint change in weekly schedule: %s",
                  (config->TemperatureSetpointChangeSwitchesToManualMode == 1)
                      ? "switches to manual"
                      : "keeps weekly");
  SUPLA_LOG_DEBUG("  UseSeparateHeatCoolOutputs: %d",
                  config->UseSeparateHeatCoolOutputs);
  SUPLA_LOG_DEBUG("  AuxMinMaxSetpointEnabled: %d",
                  config->AuxMinMaxSetpointEnabled);
  SUPLA_LOG_DEBUG("  Master thermostat: %d", config->MasterThermostatIsSet ?
                  config->MasterThermostatChannelNo : -1);
  SUPLA_LOG_DEBUG("  Heat or cold source switch channel: %d",
                  config->HeatOrColdSourceSwitchIsSet
                      ? config->HeatOrColdSourceSwitchChannelNo
                      : -1);
  SUPLA_LOG_DEBUG("  Pump switch channel: %d", config->PumpSwitchIsSet ?
                  config->PumpSwitchChannelNo : -1);
  SUPLA_LOG_DEBUG("  Temperatures:");
  for (int i = 0; i < 24; i++) {
    if ((1 << i) & config->Temperatures.Index) {
      SUPLA_LOG_DEBUG("    %s: %d",
                      temperatureName(1 << i),
                      config->Temperatures.Temperature[i]);
    }
  }
}

void HvacBase::debugPrintConfigDiff(const TChannelConfig_HVAC *configCurrent,
                                   const TChannelConfig_HVAC *configNew,
                                   int id) {
  SUPLA_LOG_DEBUG("HVAC[%d]: config diff:", id);
  bool changed = false;
  if (configCurrent->MainThermometerChannelNo !=
      configNew->MainThermometerChannelNo) {
    SUPLA_LOG_DEBUG("  Main: %d%s -> %d%s",
                    configCurrent->MainThermometerChannelNo,
                    (configCurrent->MainThermometerChannelNo == id
                     ? " (disabled)" : ""),
                    configNew->MainThermometerChannelNo,
                    (configNew->MainThermometerChannelNo == id
                     ? " (disabled)" : ""));
    changed = true;
  }
  if (configCurrent->AuxThermometerChannelNo !=
      configNew->AuxThermometerChannelNo) {
    SUPLA_LOG_DEBUG("  Aux: %d%s -> %d%s",
                    configCurrent->AuxThermometerChannelNo,
                    (configCurrent->AuxThermometerChannelNo == id
                     ? " (disabled)" : ""),
                    configNew->AuxThermometerChannelNo,
                    (configNew->AuxThermometerChannelNo == id
                     ? " (disabled)" : ""));
    changed = true;
  }
  if (configCurrent->AuxThermometerType !=
      configNew->AuxThermometerType) {
    SUPLA_LOG_DEBUG("  Aux type: %d -> %d",
                    configCurrent->AuxThermometerType,
                    configNew->AuxThermometerType);
    changed = true;
  }
  if (configCurrent->AntiFreezeAndOverheatProtectionEnabled !=
      configNew->AntiFreezeAndOverheatProtectionEnabled) {
    SUPLA_LOG_DEBUG("  AntiFreezeAndOverheatProtectionEnabled: %d -> %d",
                    configCurrent->AntiFreezeAndOverheatProtectionEnabled,
                    configNew->AntiFreezeAndOverheatProtectionEnabled);
    changed = true;
  }
  if (configCurrent->BinarySensorChannelNo !=
      configNew->BinarySensorChannelNo) {
    SUPLA_LOG_DEBUG("  BinarySensor: %d%s -> %d%s",
                    configCurrent->BinarySensorChannelNo,
                    (configCurrent->BinarySensorChannelNo == id
                     ? " (disabled)" : ""),
                    configNew->BinarySensorChannelNo,
                    (configNew->BinarySensorChannelNo == id
                     ? " (disabled)" : ""));
    changed = true;
  }
  if (configCurrent->AvailableAlgorithms != configNew->AvailableAlgorithms) {
    SUPLA_LOG_DEBUG("  Algorithms: %d -> %d",
                    configCurrent->AvailableAlgorithms,
                    configNew->AvailableAlgorithms);
    changed = true;
  }
  if (configCurrent->UsedAlgorithm != configNew->UsedAlgorithm) {
    SUPLA_LOG_DEBUG("  UsedAlgorithm: %d -> %d",
                    configCurrent->UsedAlgorithm,
                    configNew->UsedAlgorithm);
    changed = true;
  }
  if (configCurrent->MinOnTimeS != configNew->MinOnTimeS) {
    SUPLA_LOG_DEBUG("  MinOnTimeS: %d -> %d",
                    configCurrent->MinOnTimeS,
                    configNew->MinOnTimeS);
    changed = true;
  }
  if (configCurrent->MinOffTimeS != configNew->MinOffTimeS) {
    SUPLA_LOG_DEBUG("  MinOffTimeS: %d -> %d",
                    configCurrent->MinOffTimeS,
                    configNew->MinOffTimeS);
    changed = true;
  }
  if (configCurrent->OutputValueOnError != configNew->OutputValueOnError) {
    SUPLA_LOG_DEBUG("  OutputValueOnError: %d -> %d",
                    configCurrent->OutputValueOnError,
                    configNew->OutputValueOnError);
    changed = true;
  }
  if (configCurrent->Subfunction != configNew->Subfunction) {
    SUPLA_LOG_DEBUG("  Subfunction: %s (%d) -> %s (%d)",
                    (configCurrent->Subfunction == 1) ? "HEAT" :
                    (configCurrent->Subfunction == 2) ? "COOL" : "N/A",
                    configCurrent->Subfunction,
                    (configNew->Subfunction == 1) ? "HEAT" :
                    (configNew->Subfunction == 2) ? "COOL" : "N/A",
                    configNew->Subfunction);
    changed = true;
  }
  if (configCurrent->TemperatureSetpointChangeSwitchesToManualMode !=
      configNew->TemperatureSetpointChangeSwitchesToManualMode) {
    SUPLA_LOG_DEBUG(
        "  TemperatureSetpointChangeSwitchesToManualMode: %s -> %s",
        (configCurrent->TemperatureSetpointChangeSwitchesToManualMode == 1)
            ? "switches to manual"
            : "keeps weekly",
        (configNew->TemperatureSetpointChangeSwitchesToManualMode == 1)
            ? "switches to manual"
            : "keeps weekly");
    changed = true;
  }
  if (configCurrent->UseSeparateHeatCoolOutputs !=
      configNew->UseSeparateHeatCoolOutputs) {
    SUPLA_LOG_DEBUG("  UseSeparateHeatCoolOutputs: %d -> %d",
                    configCurrent->UseSeparateHeatCoolOutputs,
                    configNew->UseSeparateHeatCoolOutputs);
    changed = true;
  }
  if (configCurrent->AuxMinMaxSetpointEnabled !=
      configNew->AuxMinMaxSetpointEnabled) {
    SUPLA_LOG_DEBUG("  AuxMinMaxSetpointEnabled: %d -> %d",
                    configCurrent->AuxMinMaxSetpointEnabled,
                    configNew->AuxMinMaxSetpointEnabled);
    changed = true;
  }
  if (configCurrent->MasterThermostatIsSet !=
      configNew->MasterThermostatIsSet ||
      configCurrent->MasterThermostatChannelNo !=
      configNew->MasterThermostatChannelNo) {
    SUPLA_LOG_DEBUG("  MasterThermostat: %d%s -> %d%s",
                    configCurrent->MasterThermostatChannelNo,
                    (configCurrent->MasterThermostatIsSet ?
                     "": " (disabled)"),
                    configNew->MasterThermostatChannelNo,
                    configNew->MasterThermostatIsSet ?
                    "": " (disabled)");
    changed = true;
  }
  if (configCurrent->HeatOrColdSourceSwitchIsSet !=
      configNew->HeatOrColdSourceSwitchIsSet ||
      configCurrent->HeatOrColdSourceSwitchChannelNo !=
      configNew->HeatOrColdSourceSwitchChannelNo) {
    SUPLA_LOG_DEBUG("  HeatOrColdSourceSwitch: %d%s -> %d%s",
                    configCurrent->HeatOrColdSourceSwitchChannelNo,
                    (configCurrent->HeatOrColdSourceSwitchIsSet ?
                     "": " (disabled)"),
                    configNew->HeatOrColdSourceSwitchChannelNo,
                    configNew->HeatOrColdSourceSwitchIsSet ?
                    "": " (disabled)");
    changed = true;
  }
  if (configCurrent->PumpSwitchIsSet !=
      configNew->PumpSwitchIsSet ||
      configCurrent->PumpSwitchChannelNo !=
      configNew->PumpSwitchChannelNo) {
    SUPLA_LOG_DEBUG("  PumpSwitch: %d%s -> %d%s",
                    configCurrent->PumpSwitchChannelNo,
                    (configCurrent->PumpSwitchIsSet ?
                     "": " (disabled)"),
                    configNew->PumpSwitchChannelNo,
                    configNew->PumpSwitchIsSet ?
                    "": " (disabled)");
    changed = true;
  }
  for (int i = 0; i < 24; i++) {
    if (((1 << i) & configCurrent->Temperatures.Index ||
         (1 << i) & configNew->Temperatures.Index) &&
        (configCurrent->Temperatures.Temperature[i] !=
         configNew->Temperatures.Temperature[i])) {
      SUPLA_LOG_DEBUG(
          "  Temperature[%s]: %d%s -> %d%s",
          temperatureName(1 << i),
          configCurrent->Temperatures.Temperature[i],
          (configCurrent->Temperatures.Index & (1 << i)) ? "" : " (not set)",
          configNew->Temperatures.Temperature[i],
          (configNew->Temperatures.Index & (1 << i)) ? "" : " (not set)");
    changed = true;
    }
  }
  if (!changed) {
    SUPLA_LOG_DEBUG("  No changes");
  }
}

const char* HvacBase::temperatureName(int32_t index) {
  switch (index) {
    case TEMPERATURE_FREEZE_PROTECTION: {
      return "Freeze protection setpoint";
    }
    case TEMPERATURE_ECO: {
      return "Eco setpoint";
    }
    case TEMPERATURE_COMFORT: {
      return "Comfort setpoint";
    }
    case TEMPERATURE_BOOST: {
      return "Boost setpoint";
    }
    case TEMPERATURE_HEAT_PROTECTION: {
      return "Heat protection setpoint";
    }
    case TEMPERATURE_HISTERESIS: {
      return "Histeresis setpoint";
    }
    case TEMPERATURE_BELOW_ALARM: {
      return "Below alarm setpoint";
    }
    case TEMPERATURE_ABOVE_ALARM: {
      return "Above alarm setpoint";
    }
    case TEMPERATURE_AUX_MIN_SETPOINT: {
      return "Aux min setpoint";
    }
    case TEMPERATURE_AUX_MAX_SETPOINT: {
      return "Aux max setpoint";
    }
    case TEMPERATURE_ROOM_MIN: {
      return "Room min limit";
    }
    case TEMPERATURE_ROOM_MAX: {
      return "Room max limit";
    }
    case TEMPERATURE_AUX_MIN: {
      return "Aux min limit";
    }
    case TEMPERATURE_AUX_MAX: {
      return "Aux max limit";
    }
    case TEMPERATURE_HISTERESIS_MIN: {
      return "Histeresis min limit";
    }
    case TEMPERATURE_HISTERESIS_MAX: {
      return "Histeresis max limit";
    }
    case TEMPERATURE_HEAT_COOL_OFFSET_MIN: {
      return "Heat cool offset min limit";
    }
    case TEMPERATURE_HEAT_COOL_OFFSET_MAX: {
      return "Heat cool offset max limit";
    }
  }
  return "Unknown";
}

int32_t HvacBase::channelFunctionToIndex(int32_t channelFunction) const {
  switch (channelFunction) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
      return 1;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      return 2;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL: {
      return 3;
    }
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      return 4;
    }
  }
  return 0;
}

void HvacBase::setDefaultTemperatureRoomMin(int32_t channelFunction,
    _supla_int16_t temperature) {
  defaultTemperatureRoomMin[channelFunctionToIndex(channelFunction)] =
      temperature;
}

void HvacBase::setDefaultTemperatureRoomMax(int32_t channelFunction,
                                    _supla_int16_t temperature) {
  defaultTemperatureRoomMax[channelFunctionToIndex(channelFunction)] =
      temperature;
}

_supla_int16_t HvacBase::getDefaultTemperatureRoomMin() const {
  auto channelFunction = channel.getDefaultFunction();
  auto defaultTemperature = defaultTemperatureRoomMin[channelFunctionToIndex(
      channelFunction)];
  if (defaultTemperature == INT16_MIN) {
    defaultTemperature = defaultTemperatureRoomMin[0];
  }
  if (defaultTemperature == INT16_MIN) {
    defaultTemperature = 500;  // return 5 degree if nothing else is set
  }
  return defaultTemperature;
}

_supla_int16_t HvacBase::getDefaultTemperatureRoomMax() const {
  auto channelFunction = channel.getDefaultFunction();
  auto defaultTemperature = defaultTemperatureRoomMax[channelFunctionToIndex(
      channelFunction)];
  if (defaultTemperature == INT16_MIN) {
    defaultTemperature = defaultTemperatureRoomMax[0];
  }
  if (defaultTemperature == INT16_MIN) {
    defaultTemperature = 4000;  // return 40 degree if nothing else is set
  }
  return defaultTemperature;
}

// Default config is different from factory defaults. Default config is
// intialized when function is changed. Some parameters, like thermometer
// assignement is not modified here, while in factory default config it is
// set to predefined values.
void HvacBase::initDefaultConfig() {
  if (initialConfig) {
    memcpy(&config, initialConfig, sizeof(config));
    return;
  }

  // unregister in all aggregators
  Supla::Control::RelayHvacAggregator::UnregisterHvac(this);
  registeredInRelayHvacAggregator = false;

  TChannelConfig_HVAC newConfig = {};
  // init new config with current configuration values
  memcpy(&newConfig, &config, sizeof(newConfig));

  newConfig.AntiFreezeAndOverheatProtectionEnabled = 0;
  newConfig.AuxMinMaxSetpointEnabled = 0;

  switch (channel.getDefaultFunction()) {
    default: {
      if (isAlgorithmValid(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE)) {
        newConfig.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
      } else if (isAlgorithmValid(
                     SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST)) {
        newConfig.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST;
      } else if (isAlgorithmValid(SUPLA_HVAC_ALGORITHM_PID)) {
        newConfig.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_PID;
      }
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      // For DHW default algorithm is ON_OFF_SETPOINT_AT_MOST
      if (isAlgorithmValid(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST)) {
        newConfig.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST;
      } else if (isAlgorithmValid(
                     SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE)) {
        newConfig.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE;
      } else if (isAlgorithmValid(SUPLA_HVAC_ALGORITHM_PID)) {
        newConfig.UsedAlgorithm = SUPLA_HVAC_ALGORITHM_PID;
      }
      break;
    }
  }

  if (channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    if (defaultSubfunction != SUPLA_HVAC_SUBFUNCTION_NOT_SET) {
      newConfig.Subfunction = defaultSubfunction;
    } else {
      newConfig.Subfunction = SUPLA_HVAC_SUBFUNCTION_HEAT;
    }
  }

  newConfig.MinOffTimeS = 0;
  newConfig.MinOnTimeS = 0;
  newConfig.TemperatureSetpointChangeSwitchesToManualMode = 1;

  // some temperature config is cleared
  clearTemperatureInStruct(&newConfig.Temperatures,
                           TEMPERATURE_FREEZE_PROTECTION);
  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_ECO);
  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_COMFORT);
  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_BOOST);
  clearTemperatureInStruct(&newConfig.Temperatures,
                           TEMPERATURE_HEAT_PROTECTION);
  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_BELOW_ALARM);
  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_ABOVE_ALARM);

  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_ROOM_MIN);
  clearTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_ROOM_MAX);

  setTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_ROOM_MIN,
      getDefaultTemperatureRoomMin());
  setTemperatureInStruct(&newConfig.Temperatures, TEMPERATURE_ROOM_MAX,
      getDefaultTemperatureRoomMax());

  if (!isChannelThermometer(newConfig.MainThermometerChannelNo)) {
    newConfig.MainThermometerChannelNo = defaultMainThermometer;
  }
  if (!isChannelThermometer(newConfig.AuxThermometerChannelNo) &&
      newConfig.AuxThermometerChannelNo != getChannelNumber()) {
    newConfig.AuxThermometerChannelNo = defaultAuxThermometer;
  }
  if (!isChannelBinarySensor(newConfig.BinarySensorChannelNo) &&
      newConfig.BinarySensorChannelNo != getChannelNumber()) {
    newConfig.BinarySensorChannelNo = defaultBinarySensor;
  }

  if (defaultPumpSwitch >= 0) {
    newConfig.PumpSwitchChannelNo = defaultPumpSwitch;
    if (defaultPumpSwitch != getChannelNumber()) {
      newConfig.PumpSwitchIsSet = 1;
    }
  }
  if (defaultHeatOrColdSourceSwitch >= 0) {
    newConfig.HeatOrColdSourceSwitchChannelNo = defaultHeatOrColdSourceSwitch;
    if (defaultHeatOrColdSourceSwitch != getChannelNumber()) {
      newConfig.HeatOrColdSourceSwitchIsSet = 1;
    }
  }
  if (defaultMasterThermostat >= 0) {
    newConfig.MasterThermostatChannelNo = defaultMasterThermostat;
    if (defaultMasterThermostat != getChannelNumber()) {
      newConfig.MasterThermostatIsSet = 1;
    }
  }

  memcpy(&config, &newConfig, sizeof(config));
}

void HvacBase::initDefaultWeeklySchedule() {
  isWeeklyScheduleConfigured = true;
  // init weekly schedule to zeros. It will just have program "off" for whole
  // time.
  // We define default values for HEAT, COOL, HEAT_COOL, DOMESTIC_HOT_WATER
  // later
  memset(&weeklySchedule, 0, sizeof(weeklySchedule));
  memset(&altWeeklySchedule, 0, sizeof(altWeeklySchedule));
  bool prevInitDone = initDone;
  if (initDone) {
    weeklyScheduleChangedOffline = 1;
    initDone = false;
  }

  // first we init Program in schedule
  switch (getChannel()->getDefaultFunction()) {
    default: {
      SUPLA_LOG_WARNING(
          "HVAC[%d]: no default weekly schedule defined for function %d",
          getChannelNumber(),
          getChannel()->getDefaultFunction());
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT, 1900, 0, USE_MAIN_WEEKLYSCHEDULE);
      setProgram(2, SUPLA_HVAC_MODE_HEAT, 2100, 0, USE_MAIN_WEEKLYSCHEDULE);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, 3000, 0, USE_MAIN_WEEKLYSCHEDULE);
      setProgram(4, SUPLA_HVAC_MODE_HEAT, 1200, 0, USE_MAIN_WEEKLYSCHEDULE);

      setProgram(1, SUPLA_HVAC_MODE_COOL, 0, 2400, USE_ALT_WEEKLYSCHEDULE);
      setProgram(2, SUPLA_HVAC_MODE_COOL, 0, 2100, USE_ALT_WEEKLYSCHEDULE);
      setProgram(3, SUPLA_HVAC_MODE_COOL, 0, 1800, USE_ALT_WEEKLYSCHEDULE);
      setProgram(4, SUPLA_HVAC_MODE_COOL, 0, 2800, USE_ALT_WEEKLYSCHEDULE);
      break;
    }

    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT_COOL, 1800, 2500);
      setProgram(2, SUPLA_HVAC_MODE_HEAT_COOL, 2100, 2400);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, 2300, 0);
      setProgram(4, SUPLA_HVAC_MODE_COOL, 0, 2400);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT, -500, 0);
      setProgram(2, SUPLA_HVAC_MODE_HEAT, -200, 0);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, -1000, 0);
      setProgram(4, SUPLA_HVAC_MODE_HEAT, -1500, 0);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      setProgram(1, SUPLA_HVAC_MODE_HEAT, 4000, 0);
      setProgram(2, SUPLA_HVAC_MODE_HEAT, 5000, 0);
      setProgram(3, SUPLA_HVAC_MODE_HEAT, 3000, 0);
      setProgram(4, SUPLA_HVAC_MODE_HEAT, 6000, 0);
      break;
    }
  }

  // then we init Quarters in schedule
  auto channelFunction = channel.getDefaultFunction();
    // default schedule for heating mode
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 1;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 2;
        } else {
          program = 1;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          setWeeklySchedule(
              static_cast<enum DayOfWeek>(dayOfAWeek), hour, quarter, program,
              USE_MAIN_WEEKLYSCHEDULE);
        }
      }
    }
  }
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 0;  // off
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 1;  // cool to 24.0
        } else {
          program = 0;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          setWeeklySchedule(
              static_cast<enum DayOfWeek>(dayOfAWeek), hour, quarter, program,
              USE_ALT_WEEKLYSCHEDULE);
        }
      }
    }
  }
  if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
    for (int dayOfAWeek = 0; dayOfAWeek < 7; dayOfAWeek++) {
      int program = 1;
      for (int hour = 0; hour < 24; hour++) {
        if (hour >= 6 && hour < 21) {
          program = 2;
        } else {
          program = 1;
        }
        for (int quarter = 0; quarter < 4; quarter++) {
          setWeeklySchedule(
              static_cast<enum DayOfWeek>(dayOfAWeek), hour, quarter, program,
              USE_MAIN_WEEKLYSCHEDULE);
        }
      }
    }
  }

  initDone = prevInitDone;
  saveWeeklySchedule();
}

void HvacBase::initDefaultAlgorithm() {
  if (getUsedAlgorithm() == SUPLA_HVAC_ALGORITHM_NOT_SET) {
    if (channel.getDefaultFunction() ==
        SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER) {
      setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_AT_MOST);
    } else {
      setUsedAlgorithm(SUPLA_HVAC_ALGORITHM_ON_OFF_SETPOINT_MIDDLE);
    }
  }
}

bool HvacBase::isHeatingSubfunction() const {
  return channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
         config.Subfunction == SUPLA_HVAC_SUBFUNCTION_HEAT;
}

bool HvacBase::isCoolingSubfunction() const {
  return channel.getDefaultFunction() == SUPLA_CHANNELFNC_HVAC_THERMOSTAT &&
         config.Subfunction == SUPLA_HVAC_SUBFUNCTION_COOL;
}

void HvacBase::setDefaultSubfunction(uint8_t subfunction) {
  if (subfunction <= SUPLA_HVAC_SUBFUNCTION_COOL) {
    defaultSubfunction = subfunction;
  }
}

bool HvacBase::getForcedOffSensorState() {
  if (config.BinarySensorChannelNo != getChannelNumber()) {
    auto element =
        Supla::Element::getElementByChannelNumber(config.BinarySensorChannelNo);
    if (element == nullptr) {
      SUPLA_LOG_WARNING("HVAC[%d]: sensor not found for channel %d",
                        getChannelNumber(),
                        config.BinarySensorChannelNo);
      return false;
    }
    auto elementType = element->getChannel()->getChannelType();
    if (elementType == SUPLA_CHANNELTYPE_BINARYSENSOR) {
      // open window == false
      // missing hotel card == false
      // etc
      return element->getChannel()->getValueBool() == false;
    }
  }
  return false;
}

bool HvacBase::setBinarySensorChannelNo(uint8_t channelNo) {
  if (!initDone) {
    config.BinarySensorChannelNo = channelNo;
    defaultBinarySensor = channelNo;
    return true;
  }
  if (isChannelBinarySensor(channelNo) || channelNo == getChannelNumber()) {
    if (config.BinarySensorChannelNo != channelNo) {
      config.BinarySensorChannelNo = channelNo;
      if (initDone) {
        channelConfigChangedOffline = 1;
        saveConfig();
      }
    }
    return true;
  }
  return false;
}

uint8_t HvacBase::getBinarySensorChannelNo() const {
  return config.BinarySensorChannelNo;
}

bool HvacBase::isWeelkySchedulManualOverrideMode() const {
  return lastProgramManualOverride != -1;
}

void HvacBase::setButtonTemperatureStep(int16_t step) {
  buttonTemperatureStep = step;
}

void HvacBase::changeTemperatureSetpointsBy(int16_t tHeat, int16_t tCool) {
  auto function = getChannelFunction();
  int16_t newHeat = getTemperatureSetpointHeat() + tHeat;
  if (wrapAroundTemperatureSetpoints) {
    if (newHeat > getTemperatureRoomMax()) {
      newHeat = getTemperatureRoomMin();
    } else if (newHeat < getTemperatureRoomMin()) {
      newHeat = getTemperatureRoomMax();
    }
  }

  int16_t newCool = getTemperatureSetpointCool() + tCool;
  if (wrapAroundTemperatureSetpoints) {
    if (newCool > getTemperatureRoomMax()) {
      newCool = getTemperatureRoomMin();
    } else if (newCool < getTemperatureRoomMin()) {
      newCool = getTemperatureRoomMax();
    }
  }

  switch (function) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT: {
      if (isHeatingSubfunction()) {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_NOT_SET,
                                newHeat,
                                INT16_MIN);
      }
      if (isCoolingSubfunction()) {
        applyNewRuntimeSettings(SUPLA_HVAC_MODE_NOT_SET,
                                INT16_MIN,
                                newCool);
      }
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL:
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_NOT_SET,
                              newHeat,
                              INT16_MIN);
      break;
    }
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      applyNewRuntimeSettings(SUPLA_HVAC_MODE_NOT_SET,
                              newHeat,
                              newCool);
      break;
    }
  }
}

void HvacBase::fixTempearturesConfig() {
  auto function = getChannelFunction();

  // Aux Setpoint min and max corrections
  switch (function) {
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT:
    case SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER:
    case SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL: {
      auto tAuxSetpointMin = getTemperatureAuxMinSetpoint();
      auto tAuxSetpointMax = getTemperatureAuxMaxSetpoint();
      if (tAuxSetpointMin != INT16_MIN && tAuxSetpointMax != INT16_MIN) {
        auto tOffsetMin = getTemperatureHeatCoolOffsetMin();
        if (tOffsetMin == INT16_MIN) {
          tOffsetMin = 200;
        }
        if (tAuxSetpointMin > tAuxSetpointMax - tOffsetMin) {
          if (!setTemperatureAuxMinSetpoint(tAuxSetpointMax - tOffsetMin) &&
              !setTemperatureAuxMaxSetpoint(tAuxSetpointMin + tOffsetMin)) {
            clearTemperatureInStruct(&config.Temperatures,
                TEMPERATURE_AUX_MIN_SETPOINT);
            clearTemperatureInStruct(&config.Temperatures,
                TEMPERATURE_AUX_MAX_SETPOINT);
            setTemperatureAuxMinSetpoint(getTemperatureAuxMin());
            setTemperatureAuxMaxSetpoint(getTemperatureAuxMax());
          }
        }
      }
    }
  }

  // Anti-freeze and overheat corrections
  if (function == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
    auto tAntiFreeze = getTemperatureFreezeProtection();
    auto tOverheat = getTemperatureHeatProtection();
    if (tAntiFreeze != INT16_MIN && tOverheat != INT16_MIN) {
      auto tOffsetMin = getTemperatureHeatCoolOffsetMin();
      if (tOffsetMin == INT16_MIN) {
        tOffsetMin = 200;
      }
      if (tAntiFreeze > tOverheat - tOffsetMin) {
        if (!setTemperatureFreezeProtection(tOverheat - tOffsetMin) &&
            !setTemperatureHeatProtection(tAntiFreeze + tOffsetMin)) {
          clearTemperatureInStruct(&config.Temperatures,
              TEMPERATURE_FREEZE_PROTECTION);
          clearTemperatureInStruct(&config.Temperatures,
              TEMPERATURE_HEAT_PROTECTION);
          setTemperatureFreezeProtection(getTemperatureRoomMin());
          setTemperatureHeatProtection(getTemperatureRoomMax());
        }
      }
    }
  }
}

void HvacBase::updateTimerValue() {
  int32_t senderId = 0;
  time_t now = Supla::Clock::GetTimeStamp();
  uint32_t remainingTimeS = 0;

  if (countdownTimerEnds > now) {
    if (countdownTimerEnds - now > INT32_MAX) {
      remainingTimeS = 0;
      countdownTimerEnds = 1;
    } else {
      remainingTimeS = countdownTimerEnds - now;
    }
  }

  SUPLA_LOG_DEBUG(
      "HVAC[%d]: updating timer value: remainingTime=%d s; "
      "countdownTimerEnds=%d; now=%d",
      channel.getChannelNumber(),
      remainingTimeS,
      countdownTimerEnds,
      now);

  for (auto proto = Supla::Protocol::ProtocolLayer::first();
      proto != nullptr; proto = proto->next()) {
    proto->sendRemainingTimeValue(
        getChannelNumber(),
        remainingTimeS,
        reinterpret_cast<unsigned char*>(&lastWorkingMode),
        senderId,
        true);
  }
}

void HvacBase::clearWaitingFlags() {
  lastConfigChangeTimestampMs = 0;
  lastIterateTimestampMs = 0;
}

void HvacBase::allowWrapAroundTemperatureSetpoints() {
  wrapAroundTemperatureSetpoints = true;
}

void HvacBase::enableInitialConfig() {
  if (initialConfig == nullptr) {
    initialConfig = new TChannelConfig_HVAC;
  }
  if (initialConfig != nullptr) {
    memset(initialConfig, 0, sizeof(TChannelConfig_HVAC));
    setTemperatureInStruct(&initialConfig->Temperatures,
                           TEMPERATURE_ROOM_MIN,
                           getDefaultTemperatureRoomMin());
    setTemperatureInStruct(&initialConfig->Temperatures,
                           TEMPERATURE_ROOM_MAX,
                           getDefaultTemperatureRoomMax());

    initialConfig->MainThermometerChannelNo = defaultMainThermometer;
    initialConfig->AuxThermometerChannelNo = defaultAuxThermometer;
    initialConfig->BinarySensorChannelNo = defaultBinarySensor;
    if (defaultPumpSwitch >= 0) {
      initialConfig->PumpSwitchChannelNo = defaultPumpSwitch;
      if (defaultPumpSwitch != getChannelNumber()) {
        initialConfig->PumpSwitchIsSet = 1;
      }
    }
    if (defaultHeatOrColdSourceSwitch >= 0) {
      initialConfig->HeatOrColdSourceSwitchChannelNo =
        defaultHeatOrColdSourceSwitch;
      if (defaultHeatOrColdSourceSwitch != getChannelNumber()) {
        initialConfig->HeatOrColdSourceSwitchIsSet = 1;
      }
    }
    if (defaultMasterThermostat >= 0) {
      initialConfig->MasterThermostatChannelNo = defaultMasterThermostat;
      if (defaultMasterThermostat != getChannelNumber()) {
        initialConfig->MasterThermostatIsSet = 1;
      }
    }
  }
}

time_t HvacBase::getCountDownTimerEnds() const {
  return countdownTimerEnds;
}

void HvacBase::updateChannelState() {
  if (primaryOutput == nullptr) {
    // zero state here?
    return;
  }
  auto channelFunction = channel.getDefaultFunction();

  if (isHeatingSubfunction() ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_DOMESTIC_HOT_WATER ||
      channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_DIFFERENTIAL) {
    channel.setHvacFlagCooling(false);

    int primaryOutputValue = primaryOutput->getOutputValue();

    if (primaryOutputValue <= 0) {
      if (channel.isHvacFlagHeating()) {
        channel.setHvacFlagHeating(false);
        channel.setHvacIsOn(0);
        runAction(Supla::ON_HVAC_STANDBY);
      }
    } else {
      if (channel.getHvacIsOn() != primaryOutputValue) {
        bool runOnHvacHeating = false;
        if (!channel.isHvacFlagHeating()) {
          runOnHvacHeating = true;
        }
        channel.setHvacFlagHeating(true);
        channel.setHvacIsOn(primaryOutput->isOnOffOnly() ? 1 :
            primaryOutputValue + 1);
        if (runOnHvacHeating) {
          runAction(Supla::ON_HVAC_HEATING);
        }
      }
    }
  } else if (isCoolingSubfunction()) {
    channel.setHvacFlagHeating(false);
    auto output = primaryOutput;
    if (secondaryOutput) {
      output = secondaryOutput;
    }

    int outputValue = output->getOutputValue();

    if (outputValue >= 0) {
      if (channel.isHvacFlagCooling()) {
        channel.setHvacFlagCooling(false);
        channel.setHvacIsOn(0);
        runAction(Supla::ON_HVAC_STANDBY);
      }
    } else {
      // HvacIsOn is always 0..100 range
      if (channel.getHvacIsOn() != -outputValue) {
        bool runOnHvacCooling = false;
        if (!channel.isHvacFlagCooling()) {
          runOnHvacCooling = true;
        }
        channel.setHvacFlagCooling(true);
        channel.setHvacIsOn(output->isOnOffOnly() ? 1 : outputValue - 1);

        if (runOnHvacCooling) {
          runAction(Supla::ON_HVAC_COOLING);
        }
      }
    }
  } else if (channelFunction == SUPLA_CHANNELFNC_HVAC_THERMOSTAT_HEAT_COOL) {
    if (secondaryOutput == nullptr) {
      return;
    }

    int primaryOutputValue = primaryOutput->getOutputValue();
    int secondaryOutputValue = secondaryOutput->getOutputValue();

    if (primaryOutputValue == 0 && secondaryOutputValue == 0) {
      bool runOnHvacStandby = false;
      if (channel.isHvacFlagCooling() || channel.isHvacFlagHeating()) {
        runOnHvacStandby = true;
      }

      channel.setHvacFlagCooling(false);
      channel.setHvacFlagHeating(false);
      channel.setHvacIsOn(0);

      if (runOnHvacStandby) {
        runAction(Supla::ON_HVAC_STANDBY);
      }
    } else if (primaryOutputValue >= 1) {
      bool runOnHvacHeating = false;
      if (!channel.isHvacFlagHeating()) {
        runOnHvacHeating = true;
      }

      channel.setHvacFlagCooling(false);
      channel.setHvacFlagHeating(true);
      channel.setHvacIsOn(
          primaryOutput->isOnOffOnly() ? 1 : primaryOutputValue + 1);

      if (runOnHvacHeating) {
        runAction(Supla::ON_HVAC_HEATING);
      }
    } else if (secondaryOutputValue <= -1) {
      bool runOnHvacCooling = false;
      if (!channel.isHvacFlagCooling()) {
        runOnHvacCooling = true;
      }

      channel.setHvacFlagCooling(true);
      channel.setHvacFlagHeating(false);
      channel.setHvacIsOn(
          secondaryOutput->isOnOffOnly() ? 1 : secondaryOutputValue - 1);

      if (runOnHvacCooling) {
        runAction(Supla::ON_HVAC_COOLING);
      }
    }
  }
}

// returns true if readonly params were modified and fixed
bool HvacBase::fixReadonlyParameters(TChannelConfig_HVAC *hvacConfig) {
  if (hvacConfig == nullptr) {
    return false;
  }
  bool readonlyViolation = false;

  if (parameterFlags.MainThermometerChannelNoReadonly) {
    if (config.MainThermometerChannelNo !=
        hvacConfig->MainThermometerChannelNo) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] MainThermometerChannelNo change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.MainThermometerChannelNo,
          hvacConfig->MainThermometerChannelNo);
      hvacConfig->MainThermometerChannelNo = config.MainThermometerChannelNo;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.AuxThermometerChannelNoReadonly) {
    if (config.AuxThermometerChannelNo !=
        hvacConfig->AuxThermometerChannelNo) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] AuxThermometerChannelNo change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.AuxThermometerChannelNo,
          hvacConfig->AuxThermometerChannelNo);
      hvacConfig->AuxThermometerChannelNo = config.AuxThermometerChannelNo;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.BinarySensorChannelNoReadonly) {
    if (config.BinarySensorChannelNo != hvacConfig->BinarySensorChannelNo) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] BinarySensorChannelNo change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.BinarySensorChannelNo,
          hvacConfig->BinarySensorChannelNo);
      hvacConfig->BinarySensorChannelNo = config.BinarySensorChannelNo;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.PumpSwitchReadonly) {
    if (config.PumpSwitchChannelNo != hvacConfig->PumpSwitchChannelNo ||
        config.PumpSwitchIsSet != hvacConfig->PumpSwitchIsSet) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] PumpSwitch change from %d to %d not allowed (readonly)",
          getChannelNumber(),
          config.PumpSwitchChannelNo,
          hvacConfig->PumpSwitchChannelNo);
      hvacConfig->PumpSwitchChannelNo = config.PumpSwitchChannelNo;
      hvacConfig->PumpSwitchIsSet = config.PumpSwitchIsSet;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.HeatOrColdSourceSwitchReadonly) {
    if (config.HeatOrColdSourceSwitchChannelNo !=
            hvacConfig->HeatOrColdSourceSwitchChannelNo ||
        config.HeatOrColdSourceSwitchIsSet !=
            hvacConfig->HeatOrColdSourceSwitchIsSet) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] HeatOrColdSourceSwitch change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.HeatOrColdSourceSwitchChannelNo,
          hvacConfig->HeatOrColdSourceSwitchChannelNo);
      hvacConfig->HeatOrColdSourceSwitchChannelNo =
          config.HeatOrColdSourceSwitchChannelNo;
      hvacConfig->HeatOrColdSourceSwitchIsSet =
          config.HeatOrColdSourceSwitchIsSet;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.MasterThermostatChannelNoReadonly) {
    if (config.MasterThermostatChannelNo !=
        hvacConfig->MasterThermostatChannelNo ||
        config.MasterThermostatIsSet != hvacConfig->MasterThermostatIsSet) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] MasterThermostatChannelNo change from %d to %d not "
          "allowed "
          "(readonly)",
          getChannelNumber(),
          config.MasterThermostatChannelNo,
          hvacConfig->MasterThermostatChannelNo);
      hvacConfig->MasterThermostatChannelNo = config.MasterThermostatChannelNo;
      hvacConfig->MasterThermostatIsSet = config.MasterThermostatIsSet;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.AuxThermometerTypeReadonly) {
    if (config.AuxThermometerType != hvacConfig->AuxThermometerType) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] AuxThermometerType change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.AuxThermometerType,
          hvacConfig->AuxThermometerType);
      hvacConfig->AuxThermometerType = config.AuxThermometerType;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.AntiFreezeAndOverheatProtectionEnabledReadonly) {
    if (config.AntiFreezeAndOverheatProtectionEnabled !=
        hvacConfig->AntiFreezeAndOverheatProtectionEnabled) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] AntiFreezeAndOverheatProtection change from %d to %d not "
          "allowed "
          "(readonly)",
          getChannelNumber(),
          config.AntiFreezeAndOverheatProtectionEnabled,
          hvacConfig->AntiFreezeAndOverheatProtectionEnabled);
      hvacConfig->AntiFreezeAndOverheatProtectionEnabled =
          config.AntiFreezeAndOverheatProtectionEnabled;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.UsedAlgorithmReadonly) {
    if (config.UsedAlgorithm != hvacConfig->UsedAlgorithm) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] UsedAlgorithm change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.UsedAlgorithm,
          hvacConfig->UsedAlgorithm);
      hvacConfig->UsedAlgorithm = config.UsedAlgorithm;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.MinOnTimeSReadonly) {
    if (config.MinOnTimeS != hvacConfig->MinOnTimeS) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] MinOnTimeS change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.MinOnTimeS,
          hvacConfig->MinOnTimeS);
      hvacConfig->MinOnTimeS = config.MinOnTimeS;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.MinOffTimeSReadonly) {
    if (config.MinOffTimeS != hvacConfig->MinOffTimeS) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] MinOffTimeS change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.MinOffTimeS,
          hvacConfig->MinOffTimeS);
      hvacConfig->MinOffTimeS = config.MinOffTimeS;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.OutputValueOnErrorReadonly) {
    if (config.OutputValueOnError != hvacConfig->OutputValueOnError) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] OutputValueOnError change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.OutputValueOnError,
          hvacConfig->OutputValueOnError);
      hvacConfig->OutputValueOnError = config.OutputValueOnError;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.SubfunctionReadonly) {
    if (config.Subfunction != hvacConfig->Subfunction) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] Subfunction change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.Subfunction,
          hvacConfig->Subfunction);
      hvacConfig->Subfunction = config.Subfunction;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.TemperatureSetpointChangeSwitchesToManualModeReadonly) {
    if (config.TemperatureSetpointChangeSwitchesToManualMode !=
        hvacConfig->TemperatureSetpointChangeSwitchesToManualMode) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] TemperatureSetpointChangeSwitchesToManualMode change from "
          "%d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.TemperatureSetpointChangeSwitchesToManualMode,
          hvacConfig->TemperatureSetpointChangeSwitchesToManualMode);
      hvacConfig->TemperatureSetpointChangeSwitchesToManualMode =
          config.TemperatureSetpointChangeSwitchesToManualMode;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.AuxMinMaxSetpointEnabledReadonly) {
    if (config.AuxMinMaxSetpointEnabled !=
        hvacConfig->AuxMinMaxSetpointEnabled) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] AuxMinMaxSetpointEnabled change from %d to %d not allowed "
          "(readonly)",
          getChannelNumber(),
          config.AuxMinMaxSetpointEnabled,
          hvacConfig->AuxMinMaxSetpointEnabled);
      hvacConfig->AuxMinMaxSetpointEnabled = config.AuxMinMaxSetpointEnabled;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.UseSeparateHeatCoolOutputsReadonly) {
    if (config.UseSeparateHeatCoolOutputs !=
        hvacConfig->UseSeparateHeatCoolOutputs) {
      SUPLA_LOG_DEBUG(
          "HVAC[%d] UseSeparateHeatCoolOutputs change from %d to %d not "
          "allowed "
          "(readonly)",
          getChannelNumber(),
          config.UseSeparateHeatCoolOutputs,
          hvacConfig->UseSeparateHeatCoolOutputs);
      hvacConfig->UseSeparateHeatCoolOutputs =
          config.UseSeparateHeatCoolOutputs;
      readonlyViolation = true;
    }
  }

  if (parameterFlags.TemperaturesFreezeProtectionReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_FREEZE_PROTECTION,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesEcoReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_ECO,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesComfortReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_COMFORT,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesBoostReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_BOOST,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesHeatProtectionReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_HEAT_PROTECTION,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesHisteresisReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_HISTERESIS,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesAboveAlarmReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_ABOVE_ALARM,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesBelowAlarmReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_BELOW_ALARM,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesAuxMinSetpointReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_AUX_MIN_SETPOINT,
                           &hvacConfig->Temperatures);
  }

  if (parameterFlags.TemperaturesAuxMaxSetpointReadonly) {
    readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_AUX_MAX_SETPOINT,
                           &hvacConfig->Temperatures);
  }

  if (hvacConfig->AvailableAlgorithms != config.AvailableAlgorithms) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d] AvailableAlgorithms change from %d to %d not allowed "
        "(readonly)",
        getChannelNumber(),
        config.AvailableAlgorithms,
        hvacConfig->AvailableAlgorithms);
    hvacConfig->AvailableAlgorithms = config.AvailableAlgorithms;
    readonlyViolation = true;
  }

  readonlyViolation |=
      fixReadonlyTemperature(TEMPERATURE_ROOM_MIN, &hvacConfig->Temperatures);
  readonlyViolation |=
      fixReadonlyTemperature(TEMPERATURE_ROOM_MAX, &hvacConfig->Temperatures);
  readonlyViolation |=
      fixReadonlyTemperature(TEMPERATURE_AUX_MIN, &hvacConfig->Temperatures);
  readonlyViolation |=
      fixReadonlyTemperature(TEMPERATURE_AUX_MAX, &hvacConfig->Temperatures);
  readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_HISTERESIS_MIN,
                                              &hvacConfig->Temperatures);
  readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_HISTERESIS_MAX,
                                              &hvacConfig->Temperatures);
  readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_HEAT_COOL_OFFSET_MIN,
                                              &hvacConfig->Temperatures);
  readonlyViolation |= fixReadonlyTemperature(TEMPERATURE_HEAT_COOL_OFFSET_MAX,
                                              &hvacConfig->Temperatures);

  return readonlyViolation;
}

bool HvacBase::fixReadonlyTemperature(int32_t temperatureIndex,
                              THVACTemperatureCfg *newTemps) {
  auto currentTemperature = getTemperatureFromStruct(&config.Temperatures,
      temperatureIndex);
  auto newTemperature = getTemperatureFromStruct(newTemps, temperatureIndex);
  if (currentTemperature != newTemperature) {
    SUPLA_LOG_DEBUG(
        "HVAC[%d] Temperatures[%s] change from %d to %d not allowed (readonly)",
        getChannelNumber(),
        temperatureName(temperatureIndex),
        currentTemperature,
        newTemperature);
    if (currentTemperature == INT16_MIN) {
      clearTemperatureInStruct(newTemps, temperatureIndex);
    } else {
      setTemperatureInStruct(newTemps, temperatureIndex, currentTemperature);
    }
    return true;
  }
  return false;
}

bool HvacBase::isOutputControlledInternally() const {
  if (primaryOutput != nullptr) {
    return primaryOutput->isControlledInternally();
  }

  if (secondaryOutput != nullptr) {
    return secondaryOutput->isControlledInternally();
  }

  return false;
}

bool HvacBase::applyAdditionalValidation(TChannelConfig_HVAC *) {
  // implenent it in derived class if needed
  return false;
}

void HvacBase::stopCountDownTimer() {
  countdownTimerEnds = 1;
}

int32_t HvacBase::getRemainingCountDownTimeSec() const {
  if (countdownTimerEnds <= 1) {
    return 0;
  }
  if (!Supla::Clock::IsReady()) {
    return -1;
  }
  int32_t remainingTimeSec = countdownTimerEnds - Supla::Clock::GetTimeStamp();
  if (remainingTimeSec < 0) {
    remainingTimeSec = 0;
  }
  return remainingTimeSec;
}

bool HvacBase::setPumpSwitchChannelNo(uint8_t channelNo) {
  if (initialConfig && !initDone) {
    initialConfig->PumpSwitchChannelNo = channelNo;
    if (channelNo == getChannelNumber()) {
      initialConfig->PumpSwitchIsSet = 0;
    } else {
      initialConfig->PumpSwitchIsSet = 1;
    }
  }
  if (config.PumpSwitchChannelNo == channelNo) {
    return true;
  }

  unregisterInAggregator(config.PumpSwitchChannelNo);
  registeredInRelayHvacAggregator = false;
  if (channelNo == getChannelNumber()) {
    config.PumpSwitchIsSet = 0;
  } else {
    config.PumpSwitchIsSet = 1;
    registerInAggregator(channelNo);
  }
  config.PumpSwitchChannelNo = channelNo;
  if (!initDone) {
    defaultPumpSwitch = channelNo;
    return true;
  }
  if (initDone) {
    channelConfigChangedOffline = 1;
    saveConfig();
  }
  return true;
}

uint8_t HvacBase::getPumpSwitchChannelNo() const {
  return config.PumpSwitchChannelNo;
}

bool HvacBase::isPumpSwitchSet() const {
  return config.PumpSwitchIsSet != 0;
}

bool HvacBase::setHeatOrColdSourceSwitchChannelNo(uint8_t channelNo) {
  if (initialConfig && !initDone) {
    initialConfig->HeatOrColdSourceSwitchChannelNo = channelNo;
    if (channelNo == getChannelNumber()) {
      initialConfig->HeatOrColdSourceSwitchIsSet = 0;
    } else {
      initialConfig->HeatOrColdSourceSwitchIsSet = 1;
    }
  }
  if (config.HeatOrColdSourceSwitchChannelNo == channelNo) {
    return true;
  }

  unregisterInAggregator(config.HeatOrColdSourceSwitchChannelNo);
  registeredInRelayHvacAggregator = false;
  if (channelNo == getChannelNumber()) {
    config.HeatOrColdSourceSwitchIsSet = 0;
  } else {
    config.HeatOrColdSourceSwitchIsSet = 1;
    registerInAggregator(channelNo);
  }
  config.HeatOrColdSourceSwitchChannelNo = channelNo;
  if (!initDone) {
    defaultHeatOrColdSourceSwitch = channelNo;
    return true;
  }
  if (initDone) {
    channelConfigChangedOffline = 1;
    saveConfig();
  }
  return true;
}

uint8_t HvacBase::getHeatOrColdSourceSwitchChannelNo() const {
  return config.HeatOrColdSourceSwitchChannelNo;
}

bool HvacBase::isHeatOrColdSourceSwitchSet() const {
  return config.HeatOrColdSourceSwitchIsSet != 0;
}

bool HvacBase::setMasterThermostatChannelNo(uint8_t channelNo) {
  if (initialConfig && !initDone) {
    initialConfig->MasterThermostatChannelNo = channelNo;
    if (channelNo == getChannelNumber()) {
      initialConfig->MasterThermostatIsSet = 0;
    } else {
      initialConfig->MasterThermostatIsSet = 1;
    }
  }
  if (config.MasterThermostatChannelNo == channelNo) {
    return true;
  }
  config.MasterThermostatChannelNo = channelNo;
  if (channelNo == getChannelNumber()) {
    config.MasterThermostatIsSet = 0;
  } else {
    config.MasterThermostatIsSet = 1;
  }
  if (!initDone) {
    defaultMasterThermostat = channelNo;
    return true;
  }
  if (initDone) {
    channelConfigChangedOffline = 1;
    saveConfig();
  }
  return true;
}

uint8_t HvacBase::getMasterThermostatChannelNo() const {
  return config.MasterThermostatChannelNo;
}

bool HvacBase::isMasterThermostatSet() const {
  return config.MasterThermostatIsSet != 0;
}

void HvacBase::clearPumpSwitchChannelNo() {
  // setting to self will clear
  setPumpSwitchChannelNo(getChannelNumber());
}

void HvacBase::clearHeatOrColdSourceSwitchChannelNo() {
  // setting to self will clear
  setHeatOrColdSourceSwitchChannelNo(getChannelNumber());
}

void HvacBase::clearMasterThermostatChannelNo() {
  // setting to self will clear
  setMasterThermostatChannelNo(getChannelNumber());
}

bool HvacBase::ignoreAggregatorForRelay(int32_t relayChannelNumber) const {
  if (relayChannelNumber == -1) {
    return false;
  }
  if (defaultChannelsFlags & HVAC_BASE_FLAG_IGNORE_DEFAULT_PUMP &&
      defaultPumpSwitch >= 0 &&
      relayChannelNumber == defaultPumpSwitch) {
    return true;
  }
  if (defaultChannelsFlags & HVAC_BASE_FLAG_IGNORE_DEFAULT_HEAT_OR_COLD &&
      defaultHeatOrColdSourceSwitch >= 0 &&
      relayChannelNumber == defaultHeatOrColdSourceSwitch) {
    return true;
  }
  return false;
}

void HvacBase::setIgnoreDefaultPumpForAggregator(bool flag) {
  if (flag) {
    defaultChannelsFlags |= HVAC_BASE_FLAG_IGNORE_DEFAULT_PUMP;
  } else {
    defaultChannelsFlags &= ~HVAC_BASE_FLAG_IGNORE_DEFAULT_PUMP;
  }
}

void HvacBase::setIgnoreDefaultHeatOrColdSourceForAggregator(bool flag) {
  if (flag) {
    defaultChannelsFlags |= HVAC_BASE_FLAG_IGNORE_DEFAULT_HEAT_OR_COLD;
  } else {
    defaultChannelsFlags &= ~HVAC_BASE_FLAG_IGNORE_DEFAULT_HEAT_OR_COLD;
  }
}

bool HvacBase::registerInAggregator(int16_t channelNo) {
  if (channelNo < 0) {
    return false;
  }
  auto aggregator = Supla::Control::RelayHvacAggregator::GetInstance(channelNo);
  if (aggregator) {
    aggregator->registerHvac(this);
    return true;
  }
  return false;
}

void HvacBase::unregisterInAggregator(int16_t channelNo) {
  if (channelNo < 0) {
    return;
  }
  auto aggregator = Supla::Control::RelayHvacAggregator::GetInstance(channelNo);
  if (aggregator) {
    aggregator->unregisterHvac(this);
  }
}

void Supla::Control::HvacBase::purgeConfig() {
  Supla::ElementWithChannelActions::purgeConfig();
  auto cfg = Supla::Storage::ConfigInstance();
  if (!cfg) {
    return;
  }
  char key[SUPLA_CONFIG_MAX_KEY_SIZE] = {};
  generateKey(key, Supla::ConfigTag::HvacCfgTag);
  cfg->eraseKey(key);
  generateKey(key, Supla::ConfigTag::HvacWeeklyCfgTag);
  cfg->eraseKey(key);
  generateKey(key, Supla::ConfigTag::HvacAltWeeklyCfgTag);
  cfg->eraseKey(key);
}
