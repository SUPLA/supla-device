# This CMake file is used by gtest tests
# TODO: cleanup cmake files

set(SRCS
  supla/uptime.cpp
  supla/channels/channel.cpp
  supla/channels/binary_sensor_channel.cpp
  supla/channels/channel_extended.cpp
  supla/io.cpp
  supla/tools.cpp
  supla/element.cpp
  supla/local_action.cpp
  supla/channel_element.cpp
  supla/element_with_channel_actions.cpp
  supla/correction.cpp
  supla/at_channel.cpp
  supla/action_handler.cpp
  supla/mutex.cpp
  supla/auto_lock.cpp
  supla/crc16.cpp

  supla/device/last_state_logger.cpp
  supla/device/sw_update.cpp
  supla/device/status_led.cpp
  supla/device/remote_device_config.cpp
  supla/device/notifications.cpp
  supla/device/enter_cfg_mode_after_power_cycle.cpp

  supla/storage/storage.cpp
  supla/storage/config.cpp
  supla/storage/key_value.cpp
  supla/storage/simple_state.cpp
  supla/storage/state_storage_interface.cpp
  supla/storage/state_wear_leveling_byte.cpp

  supla/control/internal_pin_output.cpp
  supla/control/pin_status_led.cpp
  supla/control/rgbw_base.cpp
  supla/control/rgb_base.cpp
  supla/control/dimmer_base.cpp
  supla/control/rgbw_leds.cpp
  supla/control/rgb_leds.cpp
  supla/control/dimmer_leds.cpp
  supla/control/simple_button.cpp
  supla/control/button.cpp
  supla/control/sequence_button.cpp
  supla/control/action_trigger.cpp
  supla/control/relay.cpp
  supla/control/virtual_relay.cpp
  supla/control/roller_shutter.cpp
  supla/control/bistable_roller_shutter.cpp
  supla/control/tripple_button_roller_shutter.cpp
  supla/control/hvac_base.cpp
  supla/control/group_button_control_rgbw.cpp

  supla/condition.cpp
  supla/condition_getter.cpp
  supla/conditions/on_less.cpp
  supla/conditions/on_less_eq.cpp
  supla/conditions/on_greater.cpp
  supla/conditions/on_greater_eq.cpp
  supla/conditions/on_between.cpp
  supla/conditions/on_between_eq.cpp
  supla/conditions/on_equal.cpp
  supla/conditions/on_invalid.cpp

  SuplaDevice.cpp
  supla/network/network.cpp
  supla/network/web_server.cpp
  supla/network/html_element.cpp
  supla/network/html_generator.cpp
  supla/network/client.cpp
  supla/network/ip_address.cpp
  supla/network/html/text_cmd_input_parameter.cpp
  supla/network/html/select_input_parameter.cpp
  supla/network/html/rgbw_button_parameters.cpp
  supla/network/html/button_type_parameters.cpp
  supla/network/html/button_config_parameters.cpp
  supla/network/html/button_hold_time_parameters.cpp
  supla/network/html/time_parameters.cpp
  supla/network/html/volume_parameters.cpp
  supla/network/html/disable_user_interface_parameter.cpp
  supla/network/html/h2_tag.cpp
  supla/network/html/h3_tag.cpp
  supla/network/html/screen_brightness_parameters.cpp
  supla/network/html/screen_delay_parameters.cpp
  supla/network/web_sender.cpp
  supla/clock/clock.cpp

  supla/protocol/protocol_layer.cpp
  supla/protocol/supla_srpc.cpp
  supla/protocol/mqtt.cpp
  supla/protocol/mqtt_topic.cpp

  supla/sensor/therm_hygro_press_meter.cpp
  supla/sensor/therm_hygro_meter.cpp
  supla/sensor/hygro_meter.cpp
  supla/sensor/thermometer.cpp
  supla/sensor/thermometer_driver.cpp
  supla/sensor/general_purpose_channel_base.cpp
  supla/sensor/general_purpose_measurement.cpp
  supla/sensor/general_purpose_meter.cpp
  supla/sensor/memory_variable_driver.cpp
  supla/sensor/electricity_meter.cpp
  supla/sensor/binary.cpp
  supla/sensor/binary_base.cpp
  supla/sensor/virtual_binary.cpp
  supla/sensor/impulse_counter.cpp
  supla/sensor/virtual_impulse_counter.cpp
)

add_library(supladevicelib SHARED ${SRCS})
