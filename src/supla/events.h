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

#ifndef SRC_SUPLA_EVENTS_H_
#define SRC_SUPLA_EVENTS_H_

namespace Supla {

enum Event {
  // Supla::Control::Button events
  ON_PRESS,    // Triggered on transition to valueOnPress()
  ON_RELEASE,  // Triggered on transition from valueOnPress()
  ON_CHANGE,   // Triggered on all transitions
  ON_HOLD,     // Triggered when button is hold
  ON_CLICK_1,  // ON_MULTI_x is triggered when multiclick is detected
  ON_CLICK_2,
  ON_CLICK_3,
  ON_CLICK_4,
  ON_CLICK_5,
  ON_CLICK_6,
  ON_CLICK_7,
  ON_CLICK_8,
  ON_CLICK_9,
  ON_CLICK_10,
  ON_CRAZY_CLICKER,          // triggered on >= 10 clicks
  ON_SEQUENCE_MATCH,         // triggered by SequenceButton
  ON_SEQUENCE_DOESNT_MATCH,  // triggered by SequenceButton
  ON_TURN_ON,
  ON_TURN_OFF,
  ON_SECONDARY_CHANNEL_CHANGE,
  ON_LONG_CLICK_0,
  ON_LONG_CLICK_1,
  ON_LONG_CLICK_2,
  ON_LONG_CLICK_3,
  ON_LONG_CLICK_4,
  ON_LONG_CLICK_5,
  ON_LONG_CLICK_6,
  ON_LONG_CLICK_7,
  ON_LONG_CLICK_8,
  ON_LONG_CLICK_9,
  ON_LONG_CLICK_10,
  // Conditional events are triggered only when no on_hold was send
  // and we are not in the middle of muliclick detection.
  // Otherwise they work as standard events.
  // Conditional on_press is send on first press in multiclick detection
  // and on_release is send only after first button release in multiclick
  // detection (when no on_hold was send)
  CONDITIONAL_ON_PRESS,
  CONDITIONAL_ON_RELEASE,
  CONDITIONAL_ON_CHANGE,

  // SuplaDeviceClass events
  ON_DEVICE_STATUS_CHANGE,

  // Html TextCmdInputParameter
  ON_EVENT_1,
  ON_EVENT_2,
  ON_EVENT_3,
  ON_EVENT_4,
  ON_EVENT_5,
  ON_EVENT_6,
  ON_EVENT_7,
  ON_EVENT_8,
  ON_EVENT_9,
  ON_EVENT_10,

  // RGBW/DIMMER events
  ON_DIMMER_TURN_ON,            // when "brightness" changes from 0 to non-0
  ON_DIMMER_TURN_OFF,           // when "brightness" changes from non-0 to 0
  ON_DIMMER_BRIGHTNESS_CHANGE,  // on every change of "brightness"
  ON_COLOR_TURN_ON,   // when "color brightness" changes from 0 to non-0
  ON_COLOR_TURN_OFF,  // when "color brightness" changes from non-0 to 0
  ON_COLOR_BRIGHTNESS_CHANGE,  // on every change of "color brightness"
  ON_RED_TURN_ON,              // when "red" changes from 0 to non-0
  ON_RED_TURN_OFF,             // when "red" changes from non-0 to 0
  ON_RED_CHANGE,               // on every change of "red"
  ON_GREEN_TURN_ON,            // when "green" changes from 0 to non-0
  ON_GREEN_TURN_OFF,           // when "green" changes from non-0 to 0
  ON_GREEN_CHANGE,             // on every change of "green"
  ON_BLUE_TURN_ON,             // when "blue" changes from 0 to non-0
  ON_BLUE_TURN_OFF,            // when "blue" changes from non-0 to 0
  ON_BLUE_CHANGE,              // on every change of "blue"

  ON_CHANNEL_OFFLINE,
  ON_CHANNEL_ONLINE,

  ON_HVAC_HEATING,
  ON_HVAC_COOLING,
  ON_HVAC_STANDBY,

  ON_ERROR,
};

};  // namespace Supla

#endif  // SRC_SUPLA_EVENTS_H_
