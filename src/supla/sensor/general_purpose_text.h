// SPDX-FileCopyrightText: AC SOFTWARE SP. Z O.O.
// SPDX-License-Identifier: GPL-2.0-or-later

#ifndef SRC_SUPLA_SENSOR_GENERAL_PURPOSE_TEXT_H_
#define SRC_SUPLA_SENSOR_GENERAL_PURPOSE_TEXT_H_

#include <supla-common/proto.h>
#include <supla/channel_extended.h>
#include <supla/element_with_channel_actions.h>

namespace Supla {
namespace Sensor {

/**
 * General Purpose Text channel (read-only).
 *
 * The device publishes a UTF-8 text string (up to 250 bytes) which is sent to
 * the server as a TSuplaChannelExtendedValue with type
 * EV_TYPE_GENERAL_PURPOSE_TEXT. The payload starts with four metadata bytes.
 * This channel is read-only from the server.
 * The 8-byte channel value carries a monotonically increasing sequence counter
 * so the server receives the text when it changes and on periodic refresh.
 *
 * The current value is resent using the device's default 5-second interval.
 *
 * Usage example:
 *   auto *gpt = new Supla::Sensor::GeneralPurposeText();
 *   gpt->setValue("Hello, world!");
 */
class GeneralPurposeText : public ElementWithChannelActions {
 public:
  GeneralPurposeText();

  /**
   * Set the text value that will be sent to the server.
   * @param text  Null-terminated UTF-8 string (max 250 bytes).
   */
  void setValue(const char *text);

  Channel *getChannel() override;
  const Channel *getChannel() const override;

  void iterateAlways() override;
  ApplyConfigResult applyChannelConfig(TSD_ChannelConfig *result,
                                       bool local) override;
  void fillChannelConfig(void *channelConfig, int *size,
                         uint8_t configType) override;

 private:
  ChannelExtended channel;
  uint32_t lastReadTime = 0;
  bool hasTextValue = false;
};

};  // namespace Sensor
};  // namespace Supla

#endif  // SRC_SUPLA_SENSOR_GENERAL_PURPOSE_TEXT_H_
