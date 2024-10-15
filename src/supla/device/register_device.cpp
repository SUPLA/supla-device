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
#include <supla/channels/channel.h>

#include <stdio.h>

namespace {
TDS_SuplaRegisterDeviceHeader reg_dev = {};
union {
  TDS_SuplaDeviceChannel_E version_E = {};
  TDS_SuplaDeviceChannel_D version_D;
} deviceChannelStruct;
}  // namespace

#ifdef SUPLA_TEST
void Supla::RegisterDevice::resetToDefaults() {
  memset(&reg_dev, 0, sizeof(reg_dev));
  memset(
      &deviceChannelStruct.version_E, 0, sizeof(deviceChannelStruct.version_E));
}

int32_t Supla::RegisterDevice::getChannelType(int channelNumber) {
  auto channel = Supla::Channel::GetByChannelNumber(channelNumber);
  if (channel == nullptr) {
    return -1;
  }

  return channel->getChannelType();
}

int32_t Supla::RegisterDevice::getChannelDefaultFunction(int channelNumber) {
  auto channel = Supla::Channel::GetByChannelNumber(channelNumber);
  if (channel == nullptr) {
    return -1;
  }

  return channel->getDefaultFunction();
}

int32_t Supla::RegisterDevice::getChannelFunctionList(int channelNumber) {
  auto channel = Supla::Channel::GetByChannelNumber(channelNumber);
  if (channel == nullptr) {
    return -1;
  }

  return channel->getFuncList();
}

int Supla::RegisterDevice::getChannelNumber(int index) {
  if (index >= reg_dev.channel_count) {
    return -1;
  }

  int currentIndex = 0;
  Supla::Channel *ch = Supla::Channel::Begin();
  for (; ch != nullptr && currentIndex < index; ch = ch->next()) {
    currentIndex++;
  }

  if (ch == nullptr) {
    return -1;
  }

  return ch->getChannelNumber();
}

int8_t *Supla::RegisterDevice::getChannelValuePtr(int channelNumber) {
  auto ch = Supla::Channel::GetByChannelNumber(channelNumber);
  if (ch == nullptr) {
    return nullptr;
  }

  return ch->getValuePtr();
}

uint64_t Supla::RegisterDevice::getChannelFlags(int channelNumber) {
  auto ch = Supla::Channel::GetByChannelNumber(channelNumber);
  if (ch == nullptr) {
    return 0;
  }

  return ch->getFlags();
}

#endif

int Supla::RegisterDevice::getMaxChannelNumberUsed() {
  int max = -1;
  for (Supla::Channel *ch = Supla::Channel::Begin(); ch != nullptr;
      ch = ch->next()) {
    if (ch->getChannelNumber() > max) {
      max = ch->getChannelNumber();
    }
  }

  return max;
}

TDS_SuplaRegisterDeviceHeader *Supla::RegisterDevice::getRegDevHeaderPtr() {
  return &reg_dev;
}

TDS_SuplaDeviceChannel_D *Supla::RegisterDevice::getChannelPtr_D(int index) {
  if (index >= reg_dev.channel_count || index == -1) {
    return nullptr;
  }

  auto channel = Supla::Channel::Begin();
  for (int i = 0; i < reg_dev.channel_count && i < index && channel; i++) {
    channel = channel->next();
  }

  channel->fillDeviceChannelStruct(&deviceChannelStruct.version_D);

  return &deviceChannelStruct.version_D;
}

TDS_SuplaDeviceChannel_E *Supla::RegisterDevice::getChannelPtr_E(int index) {
  if (index >= reg_dev.channel_count || index == -1) {
    return nullptr;
  }

  auto channel = Supla::Channel::Begin();
  for (int i = 0; i < reg_dev.channel_count && i < index && channel; i++) {
    channel = channel->next();
  }

  channel->fillDeviceChannelStruct(&deviceChannelStruct.version_E);

  return &deviceChannelStruct.version_E;
}

bool Supla::RegisterDevice::isGUIDEmpty() {
  return isArrayEmpty(reg_dev.GUID, SUPLA_GUID_SIZE);
}

bool Supla::RegisterDevice::isAuthKeyEmpty() {
  return isArrayEmpty(reg_dev.AuthKey, SUPLA_AUTHKEY_SIZE);
}

bool Supla::RegisterDevice::isSoftVerEmpty() {
  return strnlen(reg_dev.SoftVer, SUPLA_SOFTVER_MAXSIZE) == 0;
}

bool Supla::RegisterDevice::isNameEmpty() {
  return strnlen(reg_dev.Name, SUPLA_DEVICE_NAME_MAXSIZE) == 0;
}

bool Supla::RegisterDevice::isServerNameEmpty() {
  return strnlen(reg_dev.ServerName, SUPLA_SERVER_NAME_MAXSIZE) == 0;
}

bool Supla::RegisterDevice::isEmailEmpty() {
  return strnlen(reg_dev.Email, SUPLA_EMAIL_MAXSIZE) == 0;
}

const char *Supla::RegisterDevice::getGUID() {
  return reg_dev.GUID;
}

void Supla::RegisterDevice::fillGUIDText(char text[37]) {
  // GUID format used by server:
  snprintf(
      text,
      37,
      "%02X%02X%02X%02X-%02X%02X-%02X%02X-%02X%02X-%02X%02X%02X%02X%02X%02X",
      reg_dev.GUID[0],
      reg_dev.GUID[1],
      reg_dev.GUID[2],
      reg_dev.GUID[3],
      reg_dev.GUID[4],
      reg_dev.GUID[5],
      reg_dev.GUID[6],
      reg_dev.GUID[7],
      reg_dev.GUID[8],
      reg_dev.GUID[9],
      reg_dev.GUID[10],
      reg_dev.GUID[11],
      reg_dev.GUID[12],
      reg_dev.GUID[13],
      reg_dev.GUID[14],
      reg_dev.GUID[15]);
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

// TODO(klew) move to channel
int Supla::RegisterDevice::getNextFreeChannelNumber() {
  int nextFreeNumber = 0;
  bool nextFreeFound = false;
  do {
    nextFreeFound = true;
    for (Supla::Channel *ch = Supla::Channel::Begin(); ch != nullptr;
         ch = ch->next()) {
      if (ch->getChannelNumber() == nextFreeNumber) {
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

// TODO(klew) move to channel
bool Supla::RegisterDevice::isChannelNumberFree(int channelNumber) {
  if (channelNumber >= SUPLA_CHANNELMAXCOUNT) {
    return false;
  }

  for (Supla::Channel *ch = Supla::Channel::Begin(); ch != nullptr;
       ch = ch->next()) {
    if (ch->getChannelNumber() == channelNumber) {
      return false;
    }
  }

  return true;
}

void Supla::RegisterDevice::addChannel(int channelNumber) {
  if (channelNumber >= SUPLA_CHANNELMAXCOUNT || channelNumber == -1) {
    return;
  }

  reg_dev.channel_count++;
}

void Supla::RegisterDevice::removeChannel(int channelNumber) {
  if (channelNumber >= SUPLA_CHANNELMAXCOUNT || channelNumber == -1) {
    return;
  }

  reg_dev.channel_count--;
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

bool Supla::RegisterDevice::isPairingSubdeviceEnabled() {
  return (reg_dev.Flags & SUPLA_DEVICE_FLAG_CALCFG_SUBDEVICE_PAIRING) != 0;
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
