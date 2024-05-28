/*
   Copyright (C) AC SOFTWARE SP. Z O.O

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

#include "register_device.h"

#include <supla/tools.h>
#include <string.h>
#include <stdint.h>
#include <supla/log_wrapper.h>

namespace {
TDS_SuplaRegisterDevice_E reg_dev = {};
uint8_t channelMap[SUPLA_CHANNELMAXCOUNT];
}  // namespace

#ifdef SUPLA_TEST
void Supla::RegisterDevice::resetToDefaults() {
  memset(&reg_dev, 0, sizeof(reg_dev));
  memset(channelMap, 0, sizeof(channelMap));
}
#endif

TDS_SuplaRegisterDevice_E *Supla::RegisterDevice::getRegDevPtr() {
  return &reg_dev;
}

bool Supla::RegisterDevice::isGUIDValid() {
  return !isArrayEmpty(reg_dev.GUID, SUPLA_GUID_SIZE);
}

bool Supla::RegisterDevice::isAuthKeyValid() {
  return !isArrayEmpty(reg_dev.AuthKey, SUPLA_AUTHKEY_SIZE);
}

bool Supla::RegisterDevice::isSoftVerValid() {
  return strnlen(reg_dev.SoftVer, SUPLA_SOFTVER_MAXSIZE) > 0;
}

bool Supla::RegisterDevice::isNameValid() {
  return strnlen(reg_dev.Name, SUPLA_DEVICE_NAME_MAXSIZE) > 0;
}

bool Supla::RegisterDevice::isServerNameValid() {
  return strnlen(reg_dev.ServerName, SUPLA_SERVER_NAME_MAXSIZE) > 0;
}

bool Supla::RegisterDevice::isEmailValid() {
  return strnlen(reg_dev.Email, SUPLA_EMAIL_MAXSIZE) > 0;
}

const char *Supla::RegisterDevice::getGUID() {
  return reg_dev.GUID;
}

const char *Supla::RegisterDevice::getAuthKey() {
  return reg_dev.AuthKey;
}

const char *Supla::RegisterDevice::getSoftVer() {
  return reg_dev.SoftVer;
}

const char *Supla::RegisterDevice::getName() {
  return reg_dev.Name;
}

void Supla::RegisterDevice::setGUID(const char *GUID) {
  memcpy(reg_dev.GUID, GUID, SUPLA_GUID_SIZE);
}

void Supla::RegisterDevice::setAuthKey(const char *AuthKey) {
  memcpy(reg_dev.AuthKey, AuthKey, SUPLA_AUTHKEY_SIZE);
}

void Supla::RegisterDevice::setSoftVer(const char *SoftVer) {
  strncpy(reg_dev.SoftVer, SoftVer, SUPLA_SOFTVER_MAXSIZE);
  reg_dev.SoftVer[SUPLA_SOFTVER_MAXSIZE - 1] = 0;
}

void Supla::RegisterDevice::setName(const char *Name) {
  strncpy(reg_dev.Name, Name, SUPLA_DEVICE_NAME_MAXSIZE);
  reg_dev.Name[SUPLA_DEVICE_NAME_MAXSIZE - 1] = 0;
}

void Supla::RegisterDevice::setEmail(const char *email) {
  strncpy(reg_dev.Email, email, SUPLA_EMAIL_MAXSIZE);
  reg_dev.Email[SUPLA_EMAIL_MAXSIZE - 1] = 0;
}

void Supla::RegisterDevice::setServerName(const char *server) {
  strncpy(reg_dev.ServerName, server, SUPLA_SERVER_NAME_MAXSIZE);
  reg_dev.ServerName[SUPLA_SERVER_NAME_MAXSIZE - 1] = 0;

  // for Supla public servers we check if address is misspelled (srv instead
  // of svr) and replace it
  if (Supla::RegisterDevice::isSuplaPublicServerConfigured()) {
    if (strncmpInsensitive(server, "srv", 3) == 0 && server[3] >= '0' &&
        server[3] <= '9') {
      reg_dev.ServerName[1] = 'v';
      reg_dev.ServerName[2] = 'r';
    }
  }
}

const char *Supla::RegisterDevice::getEmail() {
  return reg_dev.Email;
}

const char *Supla::RegisterDevice::getServerName() {
  return reg_dev.ServerName;
}

int Supla::RegisterDevice::getNextFreeChannelNumber() {
  int nextFreeNumber = 0;
  bool nextFreeFound = false;
  do {
    nextFreeFound = true;
    for (int i = 0; i < reg_dev.channel_count; i++) {
      if (reg_dev.channels[i].Number == nextFreeNumber) {
        nextFreeNumber++;
        nextFreeFound = false;
        break;
      }
    }
  } while (!nextFreeFound && nextFreeNumber < SUPLA_CHANNELMAXCOUNT);

  if (!nextFreeFound) {
    return -1;
  }

  return nextFreeNumber;
}

bool Supla::RegisterDevice::isChannelNumberFree(int channelNumber) {
  if (channelNumber >= SUPLA_CHANNELMAXCOUNT) {
    return false;
  }

  for (int i = 0; i < reg_dev.channel_count; i++) {
    if (reg_dev.channels[i].Number == channelNumber) {
      return false;
    }
  }

  return true;
}

void Supla::RegisterDevice::addChannel(int channelNumber) {
  if (channelNumber >= SUPLA_CHANNELMAXCOUNT) {
    return;
  }

  channelMap[channelNumber] = reg_dev.channel_count;
  memset(&reg_dev.channels[channelMap[channelNumber]], 0,
      sizeof(reg_dev.channels[channelMap[channelNumber]]));
  reg_dev.channels[channelMap[channelNumber]].Number = channelNumber;
  reg_dev.channel_count++;
}

void Supla::RegisterDevice::removeChannel(int channelNumber) {
  if (channelNumber >= SUPLA_CHANNELMAXCOUNT || channelNumber == -1) {
    return;
  }

  int channelIndex = channelMap[channelNumber];
  channelMap[channelNumber] = 0;

  int lastChannelIndex = reg_dev.channel_count - 1;
  if (channelIndex != lastChannelIndex) {
    reg_dev.channels[channelIndex] = reg_dev.channels[lastChannelIndex];
    channelMap[reg_dev.channels[channelIndex].Number] = channelIndex;
  }
  memset(&reg_dev.channels[lastChannelIndex],
         0,
         sizeof(reg_dev.channels[lastChannelIndex]));
  reg_dev.channel_count--;
}

bool Supla::RegisterDevice::setChannelNumber(int newChannelNumber,
                                             int oldChannelNumber) {
  if (newChannelNumber < 0 || oldChannelNumber < 0) {
    return false;
  }
  if (newChannelNumber == oldChannelNumber) {
    return true;
  }
  if (!Supla::RegisterDevice::isChannelNumberFree(newChannelNumber)) {
    return false;
  }

  int channelIndex = channelMap[oldChannelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return false;
  }
  channelMap[oldChannelNumber] = 0;
  channelMap[newChannelNumber] = channelIndex;
  reg_dev.channels[channelIndex].Number = newChannelNumber;
  SUPLA_LOG_INFO("Channel %d moved to %d", oldChannelNumber, newChannelNumber);
  return true;
}

bool Supla::RegisterDevice::setRawValue(int channelNumber, const void *value) {
  if (channelNumber == -1) {
    return false;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return false;
  }

  if (memcmp(value, reg_dev.channels[channelIndex].value, 8) != 0) {
    memcpy(
        reg_dev.channels[channelIndex].value, value, SUPLA_CHANNELVALUE_SIZE);
    return true;
  }
  return false;
}

bool Supla::RegisterDevice::getRawValue(int channelNumber, void *value) {
  if (channelNumber == -1) {
    return false;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return false;
  }

  memcpy(value, reg_dev.channels[channelIndex].value, SUPLA_CHANNELVALUE_SIZE);
  return true;
}

int Supla::RegisterDevice::getChannelNumber(int index) {
  if (index >= reg_dev.channel_count || index == -1) {
    return -1;
  }

  return reg_dev.channels[index].Number;
}

char *Supla::RegisterDevice::getChannelValuePtr(int channelNumber) {
  if (channelNumber == -1) {
    return nullptr;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return nullptr;
  }

  return reg_dev.channels[channelIndex].value;
}

void Supla::RegisterDevice::setChannelType(int channelNumber, int32_t type) {
  if (channelNumber == -1) {
    return;
  }
  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].Type = type;
}

int32_t Supla::RegisterDevice::getChannelType(int channelNumber) {
  if (channelNumber == -1) {
    return -1;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return -1;
  }
  return reg_dev.channels[channelIndex].Type;
}

void Supla::RegisterDevice::setChannelDefaultFunction(int channelNumber,
                                                      int32_t defaultFunction) {
  if (channelNumber == -1) {
    return;
  }
  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].Default = defaultFunction;
}

int32_t Supla::RegisterDevice::getChannelDefaultFunction(int channelNumber) {
  if (channelNumber == -1) {
    return 0;
  }
  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return 0;
  }

  return reg_dev.channels[channelIndex].Default;
}

void Supla::RegisterDevice::setChannelFlag(int channelNumber, int32_t flag) {
  if (channelNumber == -1) {
    return;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].Flags |= flag;
}

void Supla::RegisterDevice::unsetChannelFlag(int channelNumber, int32_t flag) {
    if (channelNumber == -1) {
    return;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].Flags &= ~flag;
}

int32_t Supla::RegisterDevice::getChannelFlags(int channelNumber) {
  if (channelNumber == -1) {
    return 0;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return 0;
  }
  return reg_dev.channels[channelIndex].Flags;
}

void Supla::RegisterDevice::setChannelFunctionList(int channelNumber,
                                                   int32_t functions) {
  if (channelNumber == -1) {
    return;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].FuncList = functions;
}

int32_t Supla::RegisterDevice::getChannelFunctionList(int channelNumber) {
  if (channelNumber == -1) {
    return 0;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return 0;
  }

  return reg_dev.channels[channelIndex].FuncList;
}

void Supla::RegisterDevice::addToChannelFunctionList(int channelNumber,
                                                     int32_t function) {
  if (channelNumber == -1) {
    return;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].FuncList |= function;
}

void Supla::RegisterDevice::removeFromChannelFunctionList(int channelNumber,
                                               int32_t function) {
  if (channelNumber == -1) {
    return;
  }

  int channelIndex = channelMap[channelNumber];
  if (channelIndex >= reg_dev.channel_count) {
    return;
  }
  reg_dev.channels[channelIndex].FuncList &= ~function;
}


bool Supla::RegisterDevice::isSuplaPublicServerConfigured() {
  if (reg_dev.ServerName[0] != '\0') {
    const int serverLength = strlen(reg_dev.ServerName);
    const char suplaPublicServerSuffix[] = ".supla.org";
    const int suplaPublicServerSuffixLength = strlen(suplaPublicServerSuffix);

    if (serverLength > suplaPublicServerSuffixLength) {
      if (strncmpInsensitive(reg_dev.ServerName +
            serverLength - suplaPublicServerSuffixLength,
            suplaPublicServerSuffix,
            suplaPublicServerSuffixLength) == 0) {
        return true;
      }
    }
  }
  return false;
}


void Supla::RegisterDevice::setManufacturerId(int16_t mfrId) {
  reg_dev.ManufacturerID = mfrId;
}

void Supla::RegisterDevice::setProductId(int16_t productId) {
  reg_dev.ProductID = productId;
}

void Supla::RegisterDevice::addFlags(int32_t newFlags) {
  reg_dev.Flags |= newFlags;
}

void Supla::RegisterDevice::removeFlags(int32_t removedFlags) {
  reg_dev.Flags &= ~removedFlags;
}

bool Supla::RegisterDevice::isSleepingDeviceEnabled() {
  return (reg_dev.Flags & SUPLA_DEVICE_FLAG_SLEEP_MODE_ENABLED) != 0;
}

bool Supla::RegisterDevice::isRemoteDeviceConfigEnabled() {
  return reg_dev.Flags & SUPLA_DEVICE_FLAG_DEVICE_CONFIG_SUPPORTED;
}

int16_t Supla::RegisterDevice::getManufacturerId() {
  return reg_dev.ManufacturerID;
}

int16_t Supla::RegisterDevice::getProductId() {
  return reg_dev.ProductID;
}

int Supla::RegisterDevice::getChannelCount() {
  return reg_dev.channel_count;
}
