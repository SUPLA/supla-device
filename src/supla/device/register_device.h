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

#ifndef SRC_SUPLA_DEVICE_REGISTER_DEVICE_H_
#define SRC_SUPLA_DEVICE_REGISTER_DEVICE_H_

#include <supla-common/proto.h>

#include <stdint.h>

namespace Supla {
namespace RegisterDevice {

TDS_SuplaRegisterDeviceHeader_A *getRegDevHeaderPtr();
TDS_SuplaRegisterDevice_E *getRegDevPtr();
TDS_SuplaDeviceChannel_C *getChannelPtr(int index);

// Device parameters
bool isGUIDValid();
bool isAuthKeyValid();
bool isSoftVerValid();
bool isNameValid();
bool isServerNameValid();
bool isEmailValid();

const char *getGUID();
const char *getAuthKey();
const char *getSoftVer();
const char *getName();

void setGUID(const char *GUID);
void setAuthKey(const char *AuthKey);
void setSoftVer(const char *SoftVer);
void setName(const char *Name);

void setEmail(const char *email);
void setServerName(const char *server);

const char *getEmail();
const char *getServerName();

bool isSuplaPublicServerConfigured();
bool isSleepingDeviceEnabled();
bool isRemoteDeviceConfigEnabled();

void setManufacturerId(int16_t mfrId);
void setProductId(int16_t productId);

int16_t getManufacturerId();
int16_t getProductId();

void addFlags(int32_t newFlags);
void removeFlags(int32_t removedFlags);

// Channel operations
int getNextFreeChannelNumber();
bool isChannelNumberFree(int channelNumber);
void addChannel(int channelNumber);
void removeChannel(int channelNumber);
int getChannelCount();
bool setChannelNumber(int newChannelNumber, int oldChannelNumber);

// returns true if value was changed
bool setRawValue(int channelNumber, const void *value);
// returns false on error
bool getRawValue(int channelNumber, void *value);
char *getChannelValuePtr(int channelNumber);

void setChannelType(int channelNumber, int32_t type);
int32_t getChannelType(int channelNumber);
void setChannelDefaultFunction(int channelNumber, int32_t defaultFunction);
int32_t getChannelDefaultFunction(int channelNumber);
void setChannelFlag(int channelNumber, int32_t flag);
void unsetChannelFlag(int channelNumber, int32_t flag);
int32_t getChannelFlags(int channelNumber);
void setChannelFunctionList(int channelNumber, int32_t functions);
int32_t getChannelFunctionList(int channelNumber);
void addToChannelFunctionList(int channelNumber, int32_t function);
void removeFromChannelFunctionList(int channelNumber, int32_t function);
int getChannelNumber(int index);

#ifdef SUPLA_TEST
void resetToDefaults();
#endif

}  // namespace RegisterDevice
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_REGISTER_DEVICE_H_
