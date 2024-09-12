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

#ifndef SRC_SUPLA_STORAGE_CONFIG_TAGS_H_
#define SRC_SUPLA_STORAGE_CONFIG_TAGS_H_

namespace Supla {
namespace ConfigTag {

const char ScreenBrightnessCfgTag[] = "bright";
const char ScreenAdjustmentForAutomaticCfgTag[] = "adj_auto_br";
const char PowerStatusLedCfgTag[] = "pwr_led";
const char ScreenDelayCfgTag[] = "scr_delay";
const char BtnTypeTag[] = "btn_type";
const char BtnHoldTag[] = "btn_hold";
const char VolumeCfgTag[] = "volume";
const char EmCtTypeTag[] = "em_ct";
const char RgbwButtonTag[] = "rgbw_btn";
const char BtnMulticlickTag[] = "btn_multiclick";
const char DisableUserInterfaceCfgTag[] = "disable_ui";
const char MinTempUICfgTag[] = "min_temp_ui";
const char MaxTempUICfgTag[] = "max_temp_ui";
const char BtnConfigTag[] = "btn_cfg";
const char HomeScreenContentTag[] = "home_screen";
const char ScreenDelayTypeCfgTag[] = "scr_delay_t";
const char BtnActionTriggerCfgTagPrefix[] = "mqtt_at";
const char EmPhaseLedTag[] = "em_led";
const char EmPhaseLedVoltageLowTag[] = "em_led_vl";
const char EmPhaseLedVoltageHighTag[] = "em_led_vh";
const char EmPhaseLedPowerLowTag[] = "em_led_pl";
const char EmPhaseLedPowerHighTag[] = "em_led_ph";
const char MinBrightTag[] = "min_bright";

}  // namespace ConfigTag
}  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_CONFIG_TAGS_H_
