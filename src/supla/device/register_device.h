// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_DEVICE_REGISTER_DEVICE_H_
#define SRC_SUPLA_DEVICE_REGISTER_DEVICE_H_

#include <supla-common/proto.h>

#include <stdint.h>

namespace Supla {
namespace RegisterDevice {

TDS_SuplaRegisterDeviceHeader *getRegDevHeaderPtr();
// Returns pointer to structure, which is filled with values from channel
// at given index. Next call will return exactly the same pointer, but with
// data from another channel, so please be careful when using this function
TDS_SuplaDeviceChannel_D *getChannelPtr_D(int index);
TDS_SuplaDeviceChannel_E *getChannelPtr_E(int index);

// Device parameters
bool isGUIDEmpty();
bool isAuthKeyEmpty();
bool isSoftVerEmpty();
bool isNameEmpty();
bool isServerNameEmpty();
bool isEmailEmpty();

const char *getGUID();
const char *getAuthKey();
const char *getSoftVer();
const char *getName();
void fillGUIDText(char text[37]);

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
bool isPairingSubdeviceEnabled();
bool isAutomaticFirmwareUpdateEnabled();
bool isSetCfgModePasswordEnabled();

void setManufacturerId(int16_t mfrId);
void setProductId(int16_t productId);

int16_t getManufacturerId();
int16_t getProductId();

void addFlags(int32_t newFlags);
void removeFlags(int32_t removedFlags);

// Channel operations
int getNextFreeChannelNumber();
bool isChannelNumberFree(int channelNumber);
int getFreeChannelCount();
bool hasFreeChannelCount(uint8_t requiredCount);
void addChannel(int channelNumber);
void removeChannel(int channelNumber);
int getChannelCount();
int getMaxChannelNumberUsed();

void generateHttpAgent(char *buffer, int size);

#ifdef SUPLA_TEST
void resetToDefaults();
int32_t getChannelFunctionList(int channelNumber);
int32_t getChannelDefaultFunction(int channelNumber);
int32_t getChannelType(int channelNumber);
int getChannelNumber(int index);
int8_t *getChannelValuePtr(int channelNumber);
uint64_t getChannelFlags(int channelNumber);
#endif

}  // namespace RegisterDevice
}  // namespace Supla

#endif  // SRC_SUPLA_DEVICE_REGISTER_DEVICE_H_
