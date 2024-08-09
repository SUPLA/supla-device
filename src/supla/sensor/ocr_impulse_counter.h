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

#ifndef SRC_SUPLA_SENSOR_OCR_IMPULSE_COUNTER_H_
#define SRC_SUPLA_SENSOR_OCR_IMPULSE_COUNTER_H_

#include <supla/clock/clock.h>

#include "virtual_impulse_counter.h"

namespace Supla {

namespace Sensor {
class OcrImpulseCounter : public VirtualImpulseCounter {
 public:
  OcrImpulseCounter();
  virtual ~OcrImpulseCounter();
  void onLoadState() override;
  void onSaveState() override;
  void onInit() override;
  bool iterateConnected() override;
  int handleCalcfgFromServer(TSD_DeviceCalCfgRequest *request) override;

  void addAvailableLightingMode(uint64_t mode);
  void resetCounter() override;

 protected:
  // takes photo and initialize photoDataBuffer and photoDataSize
  // returns true if photo was taken successfully
  virtual bool takePhoto() = 0;
  // release photo resource by clearing photoDataBuffer and photoDataSize
  // and calling releasePhoto for implementation specific actions
  void releasePhotoResource();
  // additional releasing of resources if needed by implementation
  virtual void releasePhoto() = 0;
  // implement turning on/off the LED. State == 0..100 %
  virtual void setLedState(int state) = 0;
  // turns on led if needed and handles delay between led turning on and photo
  bool handleLedStateBeforePhoto();
  void handleLedStateAfterPhoto();

  virtual bool sendPhotoToOcrServer(const char *url,
                                    const char *authkey,
                                    char *resultBuffer,
                                    int resultBufferSize) = 0;
  void parseServerResponse(const char *response, int responseSize);

  bool hasOcrConfig() override;
  void clearOcrConfig() override;
  bool isOcrConfigMissing() override;
  uint8_t applyChannelConfig(TSD_ChannelConfig *result, bool local) override;
  void fillChannelConfig(void *channelConfig, int *size) override;
  void fillChannelOcrConfig(void *channelConfig, int *size) override;
  void fixOcrLightingMode();

  virtual bool getStatusFromOcrServer(const char *url,
                                      const char *authkey,
                                      char *buf,
                                      int size) = 0;
  void parseStatus(const char *response, int responseSize);

  void generateUrl(char *url,
                   int urlSize,
                   const char *photoUuid = nullptr) const;

  TChannelConfig_OCR ocrConfig = {};
  bool ocrConfigReceived = false;
  uint64_t availableLightingModes = 0;
  uint32_t lastPhotoTakeTimestamp = 0;
  uint32_t lastOcrInteractionTimestamp = 0;
  uint32_t ledTurnOnTimestamp = 0;
  char lastUUIDToCheck[37] = {};
  unsigned char *photoDataBuffer = nullptr;
  int photoDataSize = 0;

  // values stored in Storage
  uint64_t lastCorrectOcrReading = 0;
  time_t lastCorrectOcrReadingTimestamp = 0;
};

}  // namespace Sensor
}  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_OCR_IMPULSE_COUNTER_H_
