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

#ifndef SRC_SUPLA_SENSOR_GENERAL_PURPOSE_TEXT_H_
#define SRC_SUPLA_SENSOR_GENERAL_PURPOSE_TEXT_H_

#include <supla-common/proto.h>
#include <supla/channel_extended.h>
#include <supla/sensor/general_purpose_channel_base.h>

namespace Supla {
namespace Sensor {

/**
 * General Purpose Text channel (read-only).
 *
 * The device publishes a UTF-8 text string (up to 255 bytes) which is sent to
 * the server as a TSuplaChannelExtendedValue with type
 * EV_TYPE_GENERAL_PURPOSE_TEXT. This channel is read-only from the server.
 * The 8-byte channel value carries a monotonically increasing sequence counter
 * so the server receives the text when it changes and on periodic refresh.
 *
 * A refresh interval of 0 uses the 5-second default.
 *
 * Usage example:
 *   auto *gpt = new Supla::Sensor::GeneralPurposeText();
 *   gpt->setValue("Hello, world!");
 */
class GeneralPurposeText : public ChannelExtended,
                           public GeneralPurposeChannelBase {
 public:
  GeneralPurposeText();

  void setInitialCaption(const char *caption,
                         bool secondaryChannel = false);

  /**
   * Set the text value that will be sent to the server.
   * @param text  Null-terminated UTF-8 string (max 255 bytes).
   */
  void setValue(const char *text);

  Channel *getChannel() override;
  const Channel *getChannel() const override;

  void onInit() override;
  void onLoadConfig(SuplaDeviceClass *sdc) override;
  void iterateAlways() override;
  ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                       bool local) override;
  void fillChannelConfig(void *channelConfig, int *size,
                         uint8_t configType) override;

 private:
  using GeneralPurposeChannelBase::setValue;
  bool hasTextValue = false;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_GENERAL_PURPOSE_TEXT_H_
