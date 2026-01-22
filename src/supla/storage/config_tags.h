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

const char DeviceConfigChangeCfgTag[] = "devcfg_chng";

const char ChannelFunctionTag[] = "fnc";
const char ChannelConfigChangedFlagTag[] = "cfg_chng";

const char ScreenBrightnessCfgTag[] = "bright";
const char ScreenAdjustmentForAutomaticCfgTag[] = "adj_auto_br";
const char HomeScreenContentTag[] = "home_screen";
const char ScreenDelayTypeCfgTag[] = "scr_delay_t";
const char ScreenDelayCfgTag[] = "scr_delay";
const char DisableUserInterfaceCfgTag[] = "disable_ui";
const char MinTempUICfgTag[] = "min_temp_ui";
const char MaxTempUICfgTag[] = "max_temp_ui";
const char MinBrightTag[] = "min_bright";

const char PowerStatusLedCfgTag[] = "pwr_led";
const char StatusLedCfgTag[] = "statusled";

const char BtnTypeTag[] = "btn_type";
const char BtnHoldTag[] = "btn_hold";
const char BtnMulticlickTag[] = "btn_multiclick";
const char BtnConfigTag[] = "btn_cfg";
const char BtnActionTriggerCfgTagPrefix[] = "mqtt_at";

const char VolumeCfgTag[] = "volume";

const char EmCtTypeTag[] = "em_ct";
const char EmPhaseLedTag[] = "em_led";
const char EmPhaseLedVoltageLowTag[] = "em_led_vl";
const char EmPhaseLedVoltageHighTag[] = "em_led_vh";
const char EmPhaseLedPowerLowTag[] = "em_led_pl";
const char EmPhaseLedPowerHighTag[] = "em_led_ph";

const char RgbwButtonTag[] = "rgbw_btn";
const char LegacyMigrationTag[] = "lgc_mig";

const char RollerShutterTag[] = "rs_cfg";
const char RollerShutterMotorUpsideDownTag[] = "usd";
const char RollerShutterButtonsUpsideDownTag[] = "bud";
const char RollerShutterTimeMarginTag[] = "rs_margin";
const char RollerShutterOpeningTimeTag[] = "rs_ot";
const char RollerShutterClosingTimeTag[] = "rs_ct";
const char FacadeBlindTiltingTimeTag[] = "fb_tilt";
const char FacadeBlindTiltControlTypeTag[] = "fb_type";
const char TiltConfigTag[] = "tilt_cfg";

const char RelayOvercurrentThreshold[] = "oc_thr";

const char HvacCfgTag[] = "hvac_cfg";
const char HvacWeeklyCfgTag[] = "hvac_weekly";
const char HvacAltWeeklyCfgTag[] = "hvac_aweekly";

const char BinarySensorServerInvertedLogicTag[] = "srv_invrt";
const char BinarySensorCfgTag[] = "bs_cfg";

const char ContainerTag[] = "container";

const char ValveCfgTag[] = "valve_cfg";

const char ModbusCfgTag[] = "modbus_cfg";

const char OtaModeTag[] = "ota_mode";

static_assert(sizeof(DeviceConfigChangeCfgTag) < 16);
static_assert(sizeof(ChannelFunctionTag) < 12);
static_assert(sizeof(ChannelConfigChangedFlagTag) < 12);
static_assert(sizeof(ScreenBrightnessCfgTag) < 16);
static_assert(sizeof(ScreenAdjustmentForAutomaticCfgTag) < 16);
static_assert(sizeof(HomeScreenContentTag) < 16);
static_assert(sizeof(ScreenDelayTypeCfgTag) < 16);
static_assert(sizeof(ScreenDelayCfgTag) < 16);
static_assert(sizeof(DisableUserInterfaceCfgTag) < 16);
static_assert(sizeof(MinTempUICfgTag) < 16);
static_assert(sizeof(MaxTempUICfgTag) < 16);
static_assert(sizeof(MinBrightTag) < 16);
static_assert(sizeof(PowerStatusLedCfgTag) < 16);
static_assert(sizeof(StatusLedCfgTag) < 16);
static_assert(sizeof(BtnTypeTag) < 16);
static_assert(sizeof(BtnHoldTag) < 16);
static_assert(sizeof(BtnMulticlickTag) < 16);
static_assert(sizeof(BtnConfigTag) < 16);
static_assert(sizeof(BtnActionTriggerCfgTagPrefix) < 16);
static_assert(sizeof(VolumeCfgTag) < 16);
static_assert(sizeof(EmCtTypeTag) < 12);
static_assert(sizeof(EmPhaseLedTag) < 12);
static_assert(sizeof(EmPhaseLedVoltageLowTag) < 12);
static_assert(sizeof(EmPhaseLedVoltageHighTag) < 12);
static_assert(sizeof(EmPhaseLedPowerLowTag) < 12);
static_assert(sizeof(EmPhaseLedPowerHighTag) < 12);
static_assert(sizeof(RgbwButtonTag) < 12);
static_assert(sizeof(RollerShutterTag) < 12);
static_assert(sizeof(RollerShutterMotorUpsideDownTag) < 12);
static_assert(sizeof(RollerShutterButtonsUpsideDownTag) < 12);
static_assert(sizeof(RollerShutterTimeMarginTag) < 12);
static_assert(sizeof(RollerShutterOpeningTimeTag) < 12);
static_assert(sizeof(RollerShutterClosingTimeTag) < 12);
static_assert(sizeof(TiltConfigTag) < 12);
static_assert(sizeof(FacadeBlindTiltingTimeTag) < 12);
static_assert(sizeof(FacadeBlindTiltControlTypeTag) < 12);
static_assert(sizeof(RelayOvercurrentThreshold) < 12);
static_assert(sizeof(HvacCfgTag) < 12);
static_assert(sizeof(HvacWeeklyCfgTag) <= 12);
static_assert(sizeof(HvacAltWeeklyCfgTag) <= 13);
static_assert(sizeof(BinarySensorServerInvertedLogicTag) < 12);
static_assert(sizeof(BinarySensorCfgTag) < 12);
static_assert(sizeof(ContainerTag) < 12);
static_assert(sizeof(ValveCfgTag) < 12);
static_assert(sizeof(ModbusCfgTag) < 16);
static_assert(sizeof(OtaModeTag) < 16);

}  // namespace ConfigTag
}  // namespace Supla

#endif  // SRC_SUPLA_STORAGE_CONFIG_TAGS_H_
