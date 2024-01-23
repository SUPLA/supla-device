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

#ifndef SRC_SUPLA_ACTIONS_H_
#define SRC_SUPLA_ACTIONS_H_

// Actions are used in ActionHandler elements. They are grouped by most common
// usage, but you should not rely on it. Please check exact supported actions
// in ActionHandler's element documentation

namespace Supla {
enum Action {
  // Relays
  TURN_ON,
  TURN_ON_WITHOUT_TIMER,  // used with staircase timer function, when
                          // timer should not be used this time
  TURN_OFF,
  TOGGLE,

  // Settable binary sensors
  SET,
  CLEAR,

  // Roller shutters
  OPEN,
  CLOSE,
  STOP,
  OPEN_OR_STOP,
  CLOSE_OR_STOP,
  COMFORT_UP_POSITION,
  COMFORT_DOWN_POSITION,
  STEP_BY_STEP,
  MOVE_UP,
  MOVE_DOWN,
  MOVE_UP_OR_STOP,
  MOVE_DOWN_OR_STOP,

  // Dimmable light, RGB(W) LEDs
  BRIGHTEN_ALL,
  DIM_ALL,
  BRIGHTEN_R,
  BRIGHTEN_G,
  BRIGHTEN_B,
  BRIGHTEN_W,
  BRIGHTEN_RGB,
  DIM_R,
  DIM_G,
  DIM_B,
  DIM_W,
  DIM_RGB,
  TURN_ON_RGB,
  TURN_OFF_RGB,
  TOGGLE_RGB,
  TURN_ON_W,
  TURN_OFF_W,
  TOGGLE_W,
  TURN_ON_RGB_DIMMED,
  TURN_ON_W_DIMMED,
  TURN_ON_ALL_DIMMED,
  ITERATE_DIM_RGB,
  ITERATE_DIM_W,
  ITERATE_DIM_ALL,

  // Impulse counter
  RESET,
  INCREMENT,
  DECREMENT,
  ENABLE,
  DISABLE,

  // Action trigger
  SEND_AT_TURN_ON,
  SEND_AT_TURN_OFF,
  SEND_AT_TOGGLE_x1,
  SEND_AT_TOGGLE_x2,
  SEND_AT_TOGGLE_x3,
  SEND_AT_TOGGLE_x4,
  SEND_AT_TOGGLE_x5,
  SEND_AT_HOLD,
  SEND_AT_SHORT_PRESS_x1,
  SEND_AT_SHORT_PRESS_x2,
  SEND_AT_SHORT_PRESS_x3,
  SEND_AT_SHORT_PRESS_x4,
  SEND_AT_SHORT_PRESS_x5,

  // SuplaDevice
  ENTER_CONFIG_MODE,
  TOGGLE_CONFIG_MODE,
  RESET_TO_FACTORY_SETTINGS,
  SOFT_RESTART,
  START_LOCAL_WEB_SERVER,
  STOP_LOCAL_WEB_SERVER,
  CHECK_SW_UPDATE,
  LEAVE_CONFIG_MODE_AND_RESET,
  ENTER_CONFIG_MODE_OR_RESET_TO_FACTORY,

  // Weight sensor
  TARE_SCALES,

  VOLUME_UP,
  VOLUME_DOWN,

  // Thermostat
  INCREASE_TEMPERATURE,
  DECREASE_TEMPERATURE,
  INCREASE_HEATING_TEMPERATURE,
  DECREASE_HEATING_TEMPERATURE,
  INCREASE_COOLING_TEMPERATURE,
  DECREASE_COOLING_TEMPERATURE,
  SWITCH_TO_MANUAL_MODE,
  SWITCH_TO_WEEKLY_SCHEDULE_MODE,
  SWITCH_TO_MANUAL_MODE_HEAT,
  SWITCH_TO_MANUAL_MODE_COOL,
  SWITCH_TO_MANUAL_MODE_HEAT_COOL,
  TOGGLE_MANUAL_WEEKLY_SCHEDULE_MODES,
  TOGGLE_OFF_MANUAL_WEEKLY_SCHEDULE_MODES,
};
};  // namespace Supla

#endif  // SRC_SUPLA_ACTIONS_H_
