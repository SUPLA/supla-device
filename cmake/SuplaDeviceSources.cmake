include_guard()

get_filename_component(SUPLA_DEVICE_ROOT_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SUPLA_DEVICE_SRC_DIR "${SUPLA_DEVICE_ROOT_DIR}/src")

set(SUPLA_DEVICE_SRCS
  ${SUPLA_DEVICE_SRC_DIR}/SuplaDevice.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/uptime.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/channels/channel.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/channels/channel_types.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/channels/binary_sensor_channel.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/channels/channel_extended.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/io.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/tools.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/element.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/local_action.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/channel_element.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/element_with_channel_actions.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/correction.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/at_channel.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/action_handler.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/time.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/timer.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/mutex.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/esp_idf_mutex.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/auto_lock.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sha256.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/crypto.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/rsa_verificator.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/crc8.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/crc16.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/log_wrapper.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/condition.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/condition_getter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_less.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_less_eq.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_greater.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_greater_eq.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_between.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_between_eq.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_equal.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/conditions/on_invalid.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/clock/clock.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/device/last_state_logger.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/status_led.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/sw_update.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/remote_device_config.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/notifications.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/enter_cfg_mode_after_power_cycle.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/register_device.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/factory_test.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/device/security_logger.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/modbus/modbus_configurator.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/modbus/modbus_client_handler.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/modbus/modbus_em_handler.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/modbus/modbus_device_handler.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/storage.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/config.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/key_value.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/simple_state.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/state_storage_interface.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/state_wear_leveling_byte.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/storage/state_wear_leveling_sector.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/network/network.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html_element.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html_generator.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/web_server.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/web_sender.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/netif_wifi.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/netif_lan.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/device_info.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/protocol_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/status_led_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/power_status_led_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/wifi_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/ethernet_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/sw_update_beta.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/sw_update.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/custom_sw_update.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/text_cmd_input_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/select_cmd_input_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/select_input_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/button_multiclick_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/button_hold_time_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/button_type_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/button_config_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/button_action_trigger_config.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/rgbw_button_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/relay_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/time_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/volume_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/screen_delay_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/screen_delay_type_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/screen_brightness_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/home_screen_content.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/disable_user_interface_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/h2_tag.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/h3_tag.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/em_phase_led.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/em_ct_type.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/roller_shutter_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/button_refresh.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/modbus_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/binary_sensor_parameters.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/security_log_list.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/custom_text_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/custom_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/custom_checkbox_parameter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/div.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/hide_show_container.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/html/channel_correction.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/network/client.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/network/ip_address.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/protocol/protocol_layer.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/protocol/supla_srpc.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/protocol/mqtt.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/protocol/mqtt_topic.cpp

  ${SUPLA_DEVICE_SRC_DIR}/supla/control/action_trigger.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/bistable_relay.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/bistable_roller_shutter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/button.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/button_aggregator.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/dimmer_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/dimmer_leds.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/internal_pin_output.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/remote_output_interface.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/light_relay.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/pin_status_led.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/relay.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/relay_hvac_aggregator.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/rgb_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/rgb_leds.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/rgbw_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/rgb_cct_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/rgbw_leds.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/roller_shutter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/roller_shutter_interface.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/sequence_button.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/simple_button.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/virtual_relay.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/hvac_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/group_button_control_rgbw.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/blinking_led.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/valve_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/virtual_valve.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/control/tripple_button_roller_shutter.cpp

  # FreeRTOS target can't compile those files currently. So they are removed
  # here and added in linux porting cmake only;
  # TODO add freertos implementation
  #  ${SUPLA_DEVICE_SRC_DIR}/supla/pv/fronius.cpp
  #  ${SUPLA_DEVICE_SRC_DIR}/supla/pv/afore.cpp

  # not all files from sensor folder are compiled here. Some still require
  # porting from ARDUINO
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/binary.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/binary_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/electricity_meter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/hygro_meter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/impulse_counter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/virtual_impulse_counter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/ocr_impulse_counter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/therm_hygro_meter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/therm_hygro_press_meter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/thermometer.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/thermometer_driver.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/general_purpose_channel_base.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/general_purpose_measurement.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/general_purpose_meter.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/memory_variable_driver.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/virtual_binary.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/ntc10k.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/weight.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/distance.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/temperature_drop_sensor.cpp
  ${SUPLA_DEVICE_SRC_DIR}/supla/sensor/container.cpp
)

function(supla_device target_name)
  target_sources(${target_name}
    PRIVATE
    ${SUPLA_DEVICE_SRCS}
    )

  target_include_directories(${target_name}
    PRIVATE
    ${SUPLA_DEVICE_SRC_DIR}
    )

  target_compile_definitions(${target_name} PUBLIC SUPLA_DEVICE)
endfunction()

