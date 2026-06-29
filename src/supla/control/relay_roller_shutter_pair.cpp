/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#include "relay_roller_shutter_pair.h"

#include <supla/log_wrapper.h>
#include <supla/storage/config.h>
#include <supla/storage/config_tags.h>
#include <supla/storage/storage.h>

namespace Supla {
namespace Control {

ManagedRelay::ManagedRelay(Supla::Io::IoPin outputPin,
                           Channel &externalChannel,
                           _supla_int_t functions)
    : Relay(outputPin, functions, externalChannel, ElementMode::Detached) {
}

void ManagedRelay::setRuntimeActive(bool active) {
  runtimeActive = active;
}

void ManagedRelay::setStorageMode(bool active) {
  storageMode = active;
}

void ManagedRelay::setLogicalState(bool state) {
  logicalState = state;
  setNewChannelValue(state);
}

void ManagedRelay::forceOff() {
  durationMs = 0;
  durationTimestamp = 0;
  logicalState = false;
  outputPin.writeInactive(channel.getChannelNumber());
  setNewChannelValue(false);
}

void ManagedRelay::loadEngineConfigOnly() {
  Relay::loadRelayConfigOnly();
}

void ManagedRelay::purgeEngineConfigOnly() {
  Relay::purgeRelayConfigOnly();
}

void ManagedRelay::turnOn(_supla_int_t duration) {
  if (!runtimeActive) {
    return;
  }
  logicalState = true;
  Relay::turnOn(duration);
}

void ManagedRelay::turnOff(_supla_int_t duration) {
  if (!runtimeActive) {
    logicalState = false;
    return;
  }
  logicalState = false;
  Relay::turnOff(duration);
}

bool ManagedRelay::isOn() {
  if (storageMode || !runtimeActive) {
    return logicalState;
  }
  logicalState = Relay::isOn();
  return logicalState;
}

void ManagedRelay::toggle(_supla_int_t duration) {
  if (!runtimeActive) {
    return;
  }
  Relay::toggle(duration);
  logicalState = Relay::isOn();
}

void ManagedRelay::handleAction(int event, int action) {
  if (!runtimeActive) {
    return;
  }
  Relay::handleAction(event, action);
}

int32_t ManagedRelay::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  if (!runtimeActive) {
    return 0;
  }
  return Relay::handleNewValueFromServer(newValue);
}

void ManagedRelay::iterateAlways() {
  if (!runtimeActive) {
    return;
  }
  Relay::iterateAlways();
}

bool ManagedRelay::iterateConnected() {
  if (!runtimeActive) {
    return true;
  }
  return Relay::iterateConnected();
}

ManagedRollerShutter::ManagedRollerShutter(Supla::Io::IoPin pinUp,
                                           Supla::Io::IoPin pinDown,
                                           bool tiltFunctionsEnabled,
                                           Channel &externalChannel)
    : RollerShutter(pinUp,
                    pinDown,
                    tiltFunctionsEnabled,
                    externalChannel,
                    ElementMode::Detached) {
}

void ManagedRollerShutter::setRuntimeActive(bool active) {
  runtimeActive = active;
}

void ManagedRollerShutter::forceStopAndSwitchOff() {
  targetPosition = STOP_POSITION;
  currentDirection = Directions::STOP_DIR;
  switchOffRelays();
}

void ManagedRollerShutter::forcePublishValue() {
  lastUpdateTime = 0;
  RollerShutter::iterateAlways();
}

void ManagedRollerShutter::loadEngineConfigOnly() {
  RollerShutter::loadRollerShutterConfigOnly();
}

void ManagedRollerShutter::purgeEngineConfigOnly() {
  RollerShutter::purgeRollerShutterConfigOnly();
}

int32_t ManagedRollerShutter::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *newValue) {
  if (!runtimeActive) {
    return 0;
  }
  return RollerShutter::handleNewValueFromServer(newValue);
}

void ManagedRollerShutter::handleAction(int event, int action) {
  if (!runtimeActive) {
    return;
  }
  RollerShutter::handleAction(event, action);
}

void ManagedRollerShutter::iterateAlways() {
  if (!runtimeActive) {
    return;
  }
  RollerShutter::iterateAlways();
}

void ManagedRollerShutter::onTimer() {
  if (!runtimeActive) {
    return;
  }
  RollerShutter::onTimer();
}

void ManagedRollerShutter::close() {
  if (runtimeActive) {
    RollerShutter::close();
  }
}

void ManagedRollerShutter::open() {
  if (runtimeActive) {
    RollerShutter::open();
  }
}

void ManagedRollerShutter::stop() {
  if (runtimeActive) {
    RollerShutter::stop();
  }
}

void ManagedRollerShutter::moveUp() {
  if (runtimeActive) {
    RollerShutter::moveUp();
  }
}

void ManagedRollerShutter::moveDown() {
  if (runtimeActive) {
    RollerShutter::moveDown();
  }
}

void ManagedRollerShutter::setTargetPosition(int newPosition, int newTilt) {
  if (runtimeActive) {
    RollerShutter::setTargetPosition(newPosition, newTilt);
  }
}

RelayRollerShutterPair::RelayRollerShutterPair(
    Supla::Io::IoPin output0,
    Supla::Io::IoPin output1,
    bool tiltFunctionsEnabled,
    _supla_int_t relayFunctions)
    : ElementWithChannelActions(ElementMode::Registered),
      relay0(output0, primaryChannel, relayFunctions),
      relay1(output1, secondaryChannel, relayFunctions),
      rollerShutter(output0, output1, tiltFunctionsEnabled, primaryChannel) {
  relayFunctions = relayOnlyFunctions(relayFunctions);
  primaryChannel.setType(SUPLA_CHANNELTYPE_RELAY);
  primaryChannel.setFuncList(primaryFunctions(relayFunctions,
                                              tiltFunctionsEnabled));
  primaryChannel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  primaryChannel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  primaryChannel.setFlag(SUPLA_CHANNEL_FLAG_RS_SBS_AND_STOP_ACTIONS);
  primaryChannel.setFlag(SUPLA_CHANNEL_FLAG_CALCFG_RECALIBRATE);
  primaryChannel.setDefaultFunction(0);

  secondaryChannel.setType(SUPLA_CHANNELTYPE_RELAY);
  secondaryChannel.setFuncList(relayFunctions);
  secondaryChannel.setFlag(SUPLA_CHANNEL_FLAG_COUNTDOWN_TIMER_SUPPORTED);
  secondaryChannel.setFlag(SUPLA_CHANNEL_FLAG_RUNTIME_CHANNEL_CONFIG_UPDATE);
  secondaryChannel.setDefaultFunction(0);

  usedConfigTypes.set(SUPLA_CONFIG_TYPE_DEFAULT);
  applyRuntimeMode();
}

RelayRollerShutterPair::RelayRollerShutterPair(Supla::Io::Base *io,
                                               int output0,
                                               int output1,
                                               bool highIsOn,
                                               bool tiltFunctionsEnabled,
                                               _supla_int_t relayFunctions)
    : RelayRollerShutterPair(makeOutputPin(io, output0, highIsOn),
                             makeOutputPin(io, output1, highIsOn),
                             tiltFunctionsEnabled,
                             relayFunctions) {
}

RelayRollerShutterPair::RelayRollerShutterPair(int output0,
                                               int output1,
                                               bool highIsOn,
                                               bool tiltFunctionsEnabled,
                                               _supla_int_t relayFunctions)
    : RelayRollerShutterPair(nullptr,
                             output0,
                             output1,
                             highIsOn,
                             tiltFunctionsEnabled,
                             relayFunctions) {
}

Supla::Io::IoPin RelayRollerShutterPair::makeOutputPin(Supla::Io::Base *io,
                                                       int pin,
                                                       bool highIsOn) {
  Supla::Io::IoPin outputPin(pin, io);
  outputPin.setActiveHigh(highIsOn);
  outputPin.setMode(OUTPUT);
  return outputPin;
}

_supla_int_t RelayRollerShutterPair::rollerFunctions(
    bool tiltFunctionsEnabled) {
  _supla_int_t result = SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER |
                        SUPLA_BIT_FUNC_CONTROLLINGTHEROOFWINDOW |
                        SUPLA_BIT_FUNC_TERRACE_AWNING |
                        SUPLA_BIT_FUNC_ROLLER_GARAGE_DOOR |
                        SUPLA_BIT_FUNC_CURTAIN |
                        SUPLA_BIT_FUNC_PROJECTOR_SCREEN;
  if (tiltFunctionsEnabled) {
    result |= SUPLA_BIT_FUNC_CONTROLLINGTHEFACADEBLIND |
              SUPLA_BIT_FUNC_VERTICAL_BLIND;
  }
  return result;
}

_supla_int_t RelayRollerShutterPair::primaryFunctions(
    _supla_int_t relayFunctions,
    bool tiltFunctionsEnabled) {
  return relayFunctions | rollerFunctions(tiltFunctionsEnabled);
}

_supla_int_t RelayRollerShutterPair::relayOnlyFunctions(
    _supla_int_t relayFunctions) {
  return relayFunctions & ~rollerFunctions(true);
}

Channel *RelayRollerShutterPair::getChannel() {
  return &primaryChannel;
}

const Channel *RelayRollerShutterPair::getChannel() const {
  return &primaryChannel;
}

Channel *RelayRollerShutterPair::getSecondaryChannel() {
  return &secondaryChannel;
}

const Channel *RelayRollerShutterPair::getSecondaryChannel() const {
  return &secondaryChannel;
}

bool RelayRollerShutterPair::isRollerFunction(uint32_t function) const {
  switch (function) {
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROLLERSHUTTER:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEROOFWINDOW:
    case SUPLA_CHANNELFNC_TERRACE_AWNING:
    case SUPLA_CHANNELFNC_ROLLER_GARAGE_DOOR:
    case SUPLA_CHANNELFNC_CURTAIN:
    case SUPLA_CHANNELFNC_PROJECTOR_SCREEN:
    case SUPLA_CHANNELFNC_CONTROLLINGTHEFACADEBLIND:
    case SUPLA_CHANNELFNC_VERTICAL_BLIND:
      return true;
    default:
      return false;
  }
}

bool RelayRollerShutterPair::isPrimaryRollerFunction() const {
  return isRollerFunction(primaryChannel.getDefaultFunction());
}

bool RelayRollerShutterPair::isPrimaryChannel(int channelNumber) const {
  return primaryChannel.getChannelNumber() == channelNumber;
}

bool RelayRollerShutterPair::isSecondaryChannel(int channelNumber) const {
  return secondaryChannel.getChannelNumber() == channelNumber;
}

void RelayRollerShutterPair::applyRuntimeMode() {
  if (isPrimaryRollerFunction()) {
    rollerModeActive = true;
    relay0.setRuntimeActive(false);
    relay1.setRuntimeActive(false);
    rollerShutter.setRuntimeActive(true);
    secondaryChannel.setStateOnlineAndNotAvailable();
  } else {
    rollerModeActive = false;
    rollerShutter.setRuntimeActive(false);
    relay0.setRuntimeActive(true);
    relay1.setRuntimeActive(true);
    secondaryChannel.setStateOnline();
  }
}

void RelayRollerShutterPair::switchToRelayMode() {
  rollerShutter.forceStopAndSwitchOff();
  relay0.forceOff();
  relay1.forceOff();
  rollerModeActive = false;
  rollerShutter.setRuntimeActive(false);
  relay0.setRuntimeActive(true);
  relay1.setRuntimeActive(true);
  secondaryChannel.setStateOnline();
  primaryChannel.clearValue();
  primaryChannel.setNewValue(false);
  secondaryChannel.clearValue();
  secondaryChannel.setNewValue(false);
}

void RelayRollerShutterPair::switchToRollerMode() {
  relay0.forceOff();
  relay1.forceOff();
  rollerShutter.forceStopAndSwitchOff();
  rollerModeActive = true;
  relay0.setRuntimeActive(false);
  relay1.setRuntimeActive(false);
  rollerShutter.setRuntimeActive(true);
  primaryChannel.clearValue();
  rollerShutter.forcePublishValue();
  secondaryChannel.clearValue();
  secondaryChannel.setStateOnlineAndNotAvailable();
}

ElementWithChannelActions *RelayRollerShutterPair::primaryActiveEngine() {
  return isPrimaryRollerFunction()
             ? static_cast<ElementWithChannelActions *>(&rollerShutter)
             : static_cast<ElementWithChannelActions *>(&relay0);
}

const ElementWithChannelActions *
RelayRollerShutterPair::primaryActiveEngine() const {
  return isPrimaryRollerFunction()
             ? static_cast<const ElementWithChannelActions *>(&rollerShutter)
             : static_cast<const ElementWithChannelActions *>(&relay0);
}

int32_t RelayRollerShutterPair::handleNewValueFromServer(
    TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return -1;
  }
  if (isPrimaryChannel(value->ChannelNumber)) {
    return isPrimaryRollerFunction()
               ? rollerShutter.handleNewValueFromServer(value)
               : relay0.handleNewValueFromServer(value);
  }
  if (isSecondaryChannel(value->ChannelNumber)) {
    if (isPrimaryRollerFunction()) {
      SUPLA_LOG_WARNING("Channel[%d] unavailable in roller shutter mode",
                        value->ChannelNumber);
      return 0;
    }
    return relay1.handleNewValueFromServer(value);
  }
  return -1;
}

void RelayRollerShutterPair::fillSuplaChannelNewValue(
    TSD_SuplaChannelNewValue *value) {
  if (value == nullptr) {
    return;
  }
  if (isPrimaryChannel(value->ChannelNumber)) {
    if (isPrimaryRollerFunction()) {
      rollerShutter.fillSuplaChannelNewValue(value);
    } else {
      relay0.fillSuplaChannelNewValue(value);
    }
  } else if (isSecondaryChannel(value->ChannelNumber) &&
             !isPrimaryRollerFunction()) {
    relay1.fillSuplaChannelNewValue(value);
  }
}

bool RelayRollerShutterPair::getRemainingCountdownTimerSec(
    uint32_t *remainingSec) const {
  if (remainingSec) {
    *remainingSec = 0;
  }
  if (isPrimaryRollerFunction()) {
    return false;
  }
  return relay0.getRemainingCountdownTimerSec(remainingSec);
}

uint8_t RelayRollerShutterPair::handleChannelConfig(TSD_ChannelConfig *config,
                                                    bool local) {
  if (config == nullptr) {
    return SUPLA_CONFIG_RESULT_DATA_ERROR;
  }
  if (isSecondaryChannel(config->ChannelNumber)) {
    if (isPrimaryRollerFunction()) {
      return SUPLA_CONFIG_RESULT_FALSE;
    }
    return relay1.handleChannelConfig(config, local);
  }
  if (isPrimaryChannel(config->ChannelNumber)) {
    return ElementWithChannelActions::handleChannelConfig(config, local);
  }
  return SUPLA_CONFIG_RESULT_FALSE;
}

void RelayRollerShutterPair::handleSetChannelConfigResult(
    TSDS_SetChannelConfigResult *result) {
  if (result == nullptr) {
    return;
  }
  if (isSecondaryChannel(result->ChannelNumber)) {
    if (!isPrimaryRollerFunction()) {
      relay1.handleSetChannelConfigResult(result);
    }
    return;
  }
  if (isPrimaryChannel(result->ChannelNumber)) {
    ElementWithChannelActions::handleSetChannelConfigResult(result);
  }
}

void RelayRollerShutterPair::handleChannelConfigFinished() {
  ElementWithChannelActions::handleChannelConfigFinished();
  if (!isPrimaryRollerFunction()) {
    relay1.handleChannelConfigFinished();
  }
}

int RelayRollerShutterPair::handleCalcfgFromServer(
    TSD_DeviceCalCfgRequest *request) {
  if (request == nullptr) {
    return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
  }
  if (isSecondaryChannel(request->ChannelNumber)) {
    return isPrimaryRollerFunction()
               ? SUPLA_CALCFG_RESULT_NOT_SUPPORTED
               : relay1.handleCalcfgFromServer(request);
  }
  if (isPrimaryChannel(request->ChannelNumber)) {
    return isPrimaryRollerFunction()
               ? rollerShutter.handleCalcfgFromServer(request)
               : relay0.handleCalcfgFromServer(request);
  }
  return SUPLA_CALCFG_RESULT_NOT_SUPPORTED;
}

uint32_t RelayRollerShutterPair::getCalcfgPendingTimeoutMs(
    TSD_DeviceCalCfgRequest *request) const {
  if (request == nullptr) {
    return 0;
  }
  if (isSecondaryChannel(request->ChannelNumber)) {
    return isPrimaryRollerFunction()
               ? 0
               : relay1.getCalcfgPendingTimeoutMs(request);
  }
  if (isPrimaryChannel(request->ChannelNumber) && isPrimaryRollerFunction()) {
    return rollerShutter.getCalcfgPendingTimeoutMs(request);
  }
  return relay0.getCalcfgPendingTimeoutMs(request);
}

void RelayRollerShutterPair::onLoadConfig(SuplaDeviceClass *sdc) {
  loadFunctionFromConfig();
  loadConfigChangeFlag();
  relay0.loadEngineConfigOnly();
  relay1.onLoadConfig(sdc);
  rollerShutter.loadEngineConfigOnly();
  applyRuntimeMode();
}

void RelayRollerShutterPair::purgeConfig() {
  ElementWithChannelActions::purgeConfig();
  relay0.purgeEngineConfigOnly();
  relay1.purgeConfig();
  rollerShutter.purgeEngineConfigOnly();
}

void RelayRollerShutterPair::onLoadState() {
  relay0.setStorageMode(true);
  relay1.setStorageMode(true);
  relay0.onLoadState();
  relay1.onLoadState();
  rollerShutter.onLoadState();
  relay0.setStorageMode(false);
  relay1.setStorageMode(false);
  applyRuntimeMode();
}

void RelayRollerShutterPair::onSaveState() {
  relay0.setStorageMode(true);
  relay1.setStorageMode(true);
  relay0.onSaveState();
  relay1.onSaveState();
  rollerShutter.onSaveState();
  relay0.setStorageMode(false);
  relay1.setStorageMode(false);
}

void RelayRollerShutterPair::onInit() {
  applyRuntimeMode();
  if (isPrimaryRollerFunction()) {
    rollerShutter.onInit();
  } else {
    relay0.onInit();
    relay1.onInit();
  }
}

void RelayRollerShutterPair::iterateAlways() {
  applyRuntimeMode();
  if (isPrimaryRollerFunction()) {
    rollerShutter.iterateAlways();
  } else {
    relay0.iterateAlways();
    relay1.iterateAlways();
  }
}

bool RelayRollerShutterPair::iterateConnected() {
  if (!ElementWithChannelActions::iterateConnected()) {
    return false;
  }
  if (!isPrimaryRollerFunction()) {
    return relay1.iterateConnected();
  }
  return true;
}

void RelayRollerShutterPair::onTimer() {
  if (isPrimaryRollerFunction()) {
    rollerShutter.onTimer();
  }
}

void RelayRollerShutterPair::onFunctionChange(uint32_t currentFunction,
                                              uint32_t newFunction) {
  (void)(currentFunction);
  if (isRollerFunction(newFunction)) {
    switchToRollerMode();
  } else {
    switchToRelayMode();
  }
}

ApplyConfigResult RelayRollerShutterPair::applyChannelConfig(
    TSD_ChannelConfig *config,
    bool local) {
  if (isPrimaryRollerFunction()) {
    return rollerShutter.applyChannelConfig(config, local);
  }
  return relay0.applyChannelConfig(config, local);
}

void RelayRollerShutterPair::fillChannelConfig(void *channelConfig,
                                               int *size,
                                               uint8_t configType) {
  if (isPrimaryRollerFunction()) {
    rollerShutter.fillChannelConfig(channelConfig, size, configType);
  } else {
    relay0.fillChannelConfig(channelConfig, size, configType);
  }
}

bool RelayRollerShutterPair::setAndSaveFunction(uint32_t channelFunction) {
  return ElementWithChannelActions::setAndSaveFunction(channelFunction);
}

}  // namespace Control
}  // namespace Supla
