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

#ifndef SRC_SUPLA_ELEMENT_H_
#define SRC_SUPLA_ELEMENT_H_

#include <supla/protocol/supla_srpc.h>

#include "channel.h"

class SuplaDeviceClass;

namespace Supla {

class Element {
 public:
  Element();
  virtual ~Element();
  static Element *begin();
  static Element *last();
  static Element *getElementByChannelNumber(int channelNumber);
  static bool IsAnyUpdatePending();
  static void NotifyElementsAboutConfigChange(uint64_t fieldBit);

  static bool IsInvalidPtrSet();
  static void ClearInvalidPtr();
  Element *next();

  // First method called on element in SuplaDevice.begin()
  // Called only if Config Storage class is configured
  // Element should read its configration in this method
  virtual void onLoadConfig(SuplaDeviceClass *sdc);

  // Second method called on element in SuplaDevice.begin()
  // method called during Config initialization (i.e. read from EEPROM, FRAM).
  // Called only if Storage class is configured
  virtual void onLoadState();

  // Third method called on element in SuplaDevice.begin()
  // method called during SuplaDevice initialization. I.e. load initial state,
  // initialize pins etc.
  virtual void onInit();

  // method called periodically during SuplaDevice iteration
  // Called only if Storage class is configured
  virtual void onSaveState();

  // method called each time when device successfully registers to server
  virtual void onRegistered(Supla::Protocol::SuplaSrpc *suplaSrpc = nullptr);

  // method called on each SuplaDevice iteration (before Network layer
  // iteration). When Device is connected, both iterateAlways() and
  // iterateConnected() are called.
  virtual void iterateAlways();

  // method called on each Supla::Device iteration when Device is connected and
  // registered to Supla server
  // ptr parameter is left for compatibility reason with previous interface
  // version
  virtual bool iterateConnected(void *ptr);
  virtual bool iterateConnected();

  // method called on timer interupt
  // Include all actions that have to be executed periodically regardless of
  // other SuplaDevice activities
  virtual void onTimer();

  // method called on fast timer interupt
  // Include all actions that have to be executed periodically regardless of
  // other SuplaDevice activities
  virtual void onFastTimer();

  // method called when soft restart is triggered
  virtual void onSoftReset();

  virtual void onDeviceConfigChange(uint64_t fieldBit);

  // return value:
  //  -1 - don't send reply to server
  //  0 - success==false
  //  1 - success==true
  virtual int handleNewValueFromServer(TSD_SuplaChannelNewValue *newValue);

  virtual void fillSuplaChannelNewValue(TSD_SuplaChannelNewValue *value);

  // Handles "get channel state" request from server
  // channelState is prefilled with network and device status informations
  virtual void handleGetChannelState(TDSC_ChannelState *channelState);

  virtual int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request);

  // Returns SUPLA_RESULTCODE_
  virtual uint8_t handleChannelConfig(TSD_ChannelConfig *newChannelConfig,
                                      bool local = false);
  virtual uint8_t handleWeeklySchedule(TSD_ChannelConfig *newWeeklySchedule,
                                       bool altSchedule = false,
                                       bool local = false);
  // handleSetChannelConfigResult should handle both standard channel config
  // and weekly schedule config
  virtual void handleSetChannelConfigResult(
      TSDS_SetChannelConfigResult *result);

  virtual void handleChannelConfigFinished();

  int getChannelNumber();
  virtual Channel *getChannel();
  virtual Channel *getSecondaryChannel();

  virtual void generateKey(char *, const char *);

  Element &disableChannelState();

 protected:
  static Element *firstPtr;
  static bool invalidatePtr;
  Element *nextPtr;
};

};  // namespace Supla

#endif  // SRC_SUPLA_ELEMENT_H_
