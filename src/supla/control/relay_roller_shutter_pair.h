/*
 Copyright (C) AC SOFTWARE SP. Z O.O.

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.
*/

#ifndef SRC_SUPLA_CONTROL_RELAY_ROLLER_SHUTTER_PAIR_H_
#define SRC_SUPLA_CONTROL_RELAY_ROLLER_SHUTTER_PAIR_H_

#include <supla/channels/channel.h>
#include <supla/control/relay.h>
#include <supla/control/roller_shutter.h>
#include <supla/io.h>

namespace Supla {
namespace Control {

class ManagedRelay : public Relay {
 public:
  ManagedRelay(Supla::Io::IoPin outputPin,
               Channel &externalChannel,
               _supla_int_t functions =
                   (0xFF ^ SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  void setRuntimeActive(bool active);
  void setStorageMode(bool active);
  void setLogicalState(bool state);
  void forceOff();
  // Loads relay-specific config only. The shared channel function must already
  // be loaded by the owner before this method is called.
  void loadEngineConfigOnly();
  void purgeEngineConfigOnly();

  void turnOn(_supla_int_t duration = 0) override;
  void turnOff(_supla_int_t duration = 0) override;
  bool isOn() override;
  void toggle(_supla_int_t duration = 0) override;
  void handleAction(int event, int action) override;
  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue)
      override;
  void iterateAlways() override;
  bool iterateConnected() override;
  bool setAndSaveFunction(uint32_t channelFunction) override;

 private:
  bool runtimeActive = true;
  bool storageMode = false;
  bool logicalState = false;
};

class ManagedRollerShutter : public RollerShutter {
 public:
  ManagedRollerShutter(Supla::Io::IoPin pinUp,
                       Supla::Io::IoPin pinDown,
                       bool tiltFunctionsEnabled,
                       Channel &externalChannel);

  void setRuntimeActive(bool active);
  void forceStopAndSwitchOff();
  void forcePublishValue();
  // Loads roller-specific config only. The shared channel function must already
  // be loaded by the owner before this method is called.
  void loadEngineConfigOnly();
  void purgeEngineConfigOnly();

  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue)
      override;
  void handleAction(int event, int action) override;
  void iterateAlways() override;
  void onTimer() override;

  void close() override;
  void open() override;
  void stop() override;
  void moveUp() override;
  void moveDown() override;
  void setTargetPosition(int newPosition,
                         int newTilt = UNKNOWN_POSITION) override;

 private:
  bool runtimeActive = true;
};

class RelayRollerShutterPair : public ElementWithChannelActions {
 public:
  RelayRollerShutterPair(Supla::Io::IoPin output0,
                         Supla::Io::IoPin output1,
                         bool tiltFunctionsEnabled = false,
                         _supla_int_t relayFunctions =
                             (0xFF ^
                              SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  RelayRollerShutterPair(Supla::Io::Base *io,
                         int output0,
                         int output1,
                         bool highIsOn = true,
                         bool tiltFunctionsEnabled = false,
                         _supla_int_t relayFunctions =
                             (0xFF ^
                              SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));
  RelayRollerShutterPair(int output0,
                         int output1,
                         bool highIsOn = true,
                         bool tiltFunctionsEnabled = false,
                         _supla_int_t relayFunctions =
                             (0xFF ^
                              SUPLA_BIT_FUNC_CONTROLLINGTHEROLLERSHUTTER));

  Channel *getChannel() override;
  const Channel *getChannel() const override;
  Channel *getSecondaryChannel() override;
  const Channel *getSecondaryChannel() const override;

  int32_t handleNewValueFromServer(TSD_SuplaChannelNewValue *value) override;
  void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value) override;
  bool getRemainingCountdownTimerSec(uint32_t *remainingSec) const override;
  uint8_t handleChannelConfig(TSD_ChannelConfig *config,
                              bool local = false) override;
  void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result) override;
  void handleChannelConfigFinished() override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;
  uint32_t getCalcfgPendingTimeoutMs(
      TSD_DeviceCalCfgRequest *request) const override;

  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void purgeConfig() override;
  void onLoadState() override;
  void onSaveState() override;
  void onInit() override;
  void iterateAlways() override;
  bool iterateConnected() override;
  void onTimer() override;
  void onFunctionChange(uint32_t currentFunction,
                        uint32_t newFunction) override;

  ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *config,
                                       bool local = false) override;
  void fillChannelConfig(void *channelConfig,
                         int *size,
                         uint8_t configType) override;

 protected:
  bool setAndSaveFunction(uint32_t channelFunction) override;

 private:
  static Supla::Io::IoPin makeOutputPin(Supla::Io::Base *io,
                                        int pin,
                                        bool highIsOn);
  static _supla_int_t rollerFunctions(bool tiltFunctionsEnabled);
  static _supla_int_t relayOnlyFunctions(_supla_int_t relayFunctions);
  static _supla_int_t primaryFunctions(_supla_int_t relayFunctions,
                                       bool tiltFunctionsEnabled);

  bool isPrimaryRollerFunction() const;
  bool isRollerFunction(uint32_t function) const;
  bool isPrimaryChannel(int channelNumber) const;
  bool isSecondaryChannel(int channelNumber) const;
  void switchToRelayMode();
  void switchToRollerMode();
  void applyRuntimeMode();
  ElementWithChannelActions *primaryActiveEngine();
  const ElementWithChannelActions *primaryActiveEngine() const;

  Channel primaryChannel;
  Channel secondaryChannel;

  ManagedRelay relay0;
  ManagedRelay relay1;
  ManagedRollerShutter rollerShutter;

  bool rollerModeActive = false;
};

}  // namespace Control
}  // namespace Supla

#endif  // SRC_SUPLA_CONTROL_RELAY_ROLLER_SHUTTER_PAIR_H_
