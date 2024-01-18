set(SUPLA_COMMONM_SRCS
  ../../../src/supla-common/lck.c
  ../../../src/supla-common/log.c
  ../../../src/supla-common/proto.c
  ../../../src/supla-common/srpc.c
)

set(SUPLA_DEVICE_SRCS
  ../../../src/SuplaDevice.cpp

  ../../../src/supla/action_handler.cpp
  ../../../src/supla/at_channel.cpp
  ../../../src/supla/channels/channel.cpp
  ../../../src/supla/channels/binary_sensor_channel.cpp
  ../../../src/supla/channel_element.cpp
  ../../../src/supla/element_with_channel_actions.cpp
  ../../../src/supla/channels/channel_extended.cpp
  ../../../src/supla/correction.cpp
  ../../../src/supla/element.cpp
  ../../../src/supla/io.cpp
  ../../../src/supla/local_action.cpp
  ../../../src/supla/log_wrapper.cpp
  ../../../src/supla/time.cpp
  ../../../src/supla/timer.cpp
  ../../../src/supla/tools.cpp
  ../../../src/supla/uptime.cpp
  ../../../src/supla/mutex.cpp
  ../../../src/supla/auto_lock.cpp
  ../../../src/supla/sha256.cpp
  ../../../src/supla/rsa_verificator.cpp
  ../../../src/supla/crc8.cpp
  ../../../src/supla/crc16.cpp

  ../../../src/supla/storage/storage.cpp
  ../../../src/supla/storage/config.cpp
  ../../../src/supla/storage/simple_state.cpp
  ../../../src/supla/storage/state_storage_interface.cpp
  ../../../src/supla/storage/state_wear_leveling_byte.cpp

  ../../../src/supla/network/network.cpp
  ../../../src/supla/network/netif_wifi.cpp
  ../../../src/supla/network/web_server.cpp
  ../../../src/supla/network/web_sender.cpp
  ../../../src/supla/network/html_generator.cpp
  ../../../src/supla/network/html_element.cpp
  ../../../src/supla/network/html/device_info.cpp
  ../../../src/supla/network/html/wifi_parameters.cpp
  ../../../src/supla/network/html/protocol_parameters.cpp
  ../../../src/supla/network/html/status_led_parameters.cpp
  ../../../src/supla/network/html/custom_sw_update.cpp
  ../../../src/supla/network/html/sw_update_beta.cpp
  ../../../src/supla/network/html/sw_update.cpp
  ../../../src/supla/network/html/button_multiclick_parameters.cpp
  ../../../src/supla/network/html/custom_parameter.cpp
  ../../../src/supla/network/html/channel_correction.cpp
  ../../../src/supla/network/html/hide_show_container.cpp
  ../../../src/supla/network/html/div.cpp
  ../../../src/supla/network/html/button_action_trigger_config.cpp
  ../../../src/supla/network/html/text_cmd_input_parameter.cpp
  ../../../src/supla/network/html/select_cmd_input_parameter.cpp
  ../../../src/supla/network/html/hvac_parameters.cpp
  ../../../src/supla/network/html/select_input_parameter.cpp
  ../../../src/supla/network/html/button_multiclick_parameters.cpp
  ../../../src/supla/network/html/button_hold_time_parameters.cpp
  ../../../src/supla/network/html/button_type_parameters.cpp
  ../../../src/supla/network/html/button_config_parameters.cpp
  ../../../src/supla/network/html/rgbw_button_parameters.cpp
  ../../../src/supla/network/html/time_parameters.cpp
  ../../../src/supla/network/html/volume_parameters.cpp
  ../../../src/supla/network/html/screen_delay_parameters.cpp
  ../../../src/supla/network/html/disable_user_interface_parameter.cpp
  ../../../src/supla/network/html/h2_tag.cpp
  ../../../src/supla/network/html/h3_tag.cpp
  ../../../src/supla/network/html/screen_brightness_parameters.cpp
  ../../../src/supla/network/client.cpp
  ../../../src/supla/network/ip_address.cpp

  ../../../src/supla/protocol/protocol_layer.cpp
  ../../../src/supla/protocol/supla_srpc.cpp
  ../../../src/supla/protocol/mqtt.cpp
  ../../../src/supla/protocol/mqtt_topic.cpp

  ../../../src/supla/clock/clock.cpp

  ../../../src/supla/control/action_trigger.cpp
  ../../../src/supla/control/bistable_relay.cpp
  ../../../src/supla/control/bistable_roller_shutter.cpp
  ../../../src/supla/control/button.cpp
  ../../../src/supla/control/dimmer_base.cpp
  ../../../src/supla/control/dimmer_leds.cpp
  ../../../src/supla/control/internal_pin_output.cpp
  ../../../src/supla/control/light_relay.cpp
  ../../../src/supla/control/pin_status_led.cpp
  ../../../src/supla/control/relay.cpp
  ../../../src/supla/control/rgb_base.cpp
  ../../../src/supla/control/rgb_leds.cpp
  ../../../src/supla/control/rgbw_base.cpp
  ../../../src/supla/control/rgbw_leds.cpp
  ../../../src/supla/control/roller_shutter.cpp
  ../../../src/supla/control/sequence_button.cpp
  ../../../src/supla/control/simple_button.cpp
  ../../../src/supla/control/virtual_relay.cpp
  ../../../src/supla/control/hvac_base.cpp
  ../../../src/supla/control/group_button_control_rgbw.cpp

  ../../../src/supla/condition.cpp
  ../../../src/supla/condition_getter.cpp
  ../../../src/supla/conditions/on_less.cpp
  ../../../src/supla/conditions/on_less_eq.cpp
  ../../../src/supla/conditions/on_greater.cpp
  ../../../src/supla/conditions/on_greater_eq.cpp
  ../../../src/supla/conditions/on_between.cpp
  ../../../src/supla/conditions/on_between_eq.cpp
  ../../../src/supla/conditions/on_equal.cpp
  ../../../src/supla/conditions/on_invalid.cpp

  ../../../src/supla/device/status_led.cpp
  ../../../src/supla/device/last_state_logger.cpp
  ../../../src/supla/device/sw_update.cpp
  ../../../src/supla/device/factory_test.cpp
  ../../../src/supla/device/remote_device_config.cpp
  ../../../src/supla/device/enter_cfg_mode_after_power_cycle.cpp
# not all files from sensor folder are compiled here. Some still require
# porting from ARDUINO
  ../../../src/supla/sensor/binary.cpp
  ../../../src/supla/sensor/binary_base.cpp
  ../../../src/supla/sensor/electricity_meter.cpp
  ../../../src/supla/sensor/hygro_meter.cpp
  ../../../src/supla/sensor/impulse_counter.cpp
  ../../../src/supla/sensor/therm_hygro_meter.cpp
  ../../../src/supla/sensor/therm_hygro_press_meter.cpp
  ../../../src/supla/sensor/thermometer.cpp
  ../../../src/supla/sensor/thermometer_driver.cpp
  ../../../src/supla/sensor/general_purpose_channel_base.cpp
  ../../../src/supla/sensor/general_purpose_meter.cpp
  ../../../src/supla/sensor/memory_variable_driver.cpp
  ../../../src/supla/sensor/virtual_binary.cpp
  ../../../src/supla/sensor/distance.cpp
  ../../../src/supla/sensor/HC_SR04.cpp
  ../../../src/supla/sensor/ntc10k.cpp
  ../../../src/supla/pv/fronius.cpp
)

set(NETTLE_SRCS
  ../../../src/nettle/bignum.c
  ../../../src/nettle/gmp-glue.c
  ../../../src/nettle/mini-gmp.c
  ../../../src/nettle/pkcs1-rsa-sha256.c
  ../../../src/nettle/pkcs1.c
  ../../../src/nettle/rsa-sha256-verify.c
  ../../../src/nettle/rsa-verify.c
  ../../../src/nettle/rsa.c
  ../../../src/nettle/sha256-compress.c
  ../../../src/nettle/sha256.c
  ../../../src/nettle/write-be32.c
  )


set(SUPLA_DEVICE_PORTING_SRCS
  ../../porting/esp-idf/esp_platform.cpp
  ../../porting/esp-idf/esp_idf_wifi.cpp
  ../../porting/esp-idf/esp_idf_gpio.cpp
  ../../porting/esp-idf/spiffs_storage.cpp
  ../../porting/esp-idf/nvs_config.cpp
  ../../porting/esp-idf/esp_idf_web_server.cpp
  ../../porting/esp-idf/esp_idf_mutex.cpp
  ../../porting/esp-idf/esp_idf_ota.cpp
  ../../porting/esp-idf/esp_idf_client.cpp
  ../../porting/esp-idf/esp_mqtt.cpp
)


idf_component_register(SRCS
  ${SUPLA_COMMONM_SRCS}
  ${SUPLA_DEVICE_SRCS}
  ${NETTLE_SRCS}
  ${SUPLA_DEVICE_PORTING_SRCS}
  INCLUDE_DIRS "../../../src/" "../../porting/esp-idf"
  REQUIRES
    esp-tls
    nvs_flash
    spiffs
    esp_http_server
    app_update
    esp_http_client
    json
    esp_wifi
    driver
    mqtt
    esp_timer
  EMBED_TXTFILES supla_org_cert.pem supla_3rd_cert.pem
)

target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DEVICE)
target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DEVICE_ESP32)
if (CONFIG_SUPLA_DEBUG_BUILD)
  target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DEBUG)
  #add_definitions( -DSUPLA_DEBUG )
  if (CONFIG_SUPLA_DISABLE_LOGS)
    target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DISABLE_LOGS)
    #add_definitions( -DSUPLA_DISABLE_LOGS )
  endif()
else()
  target_compile_definitions(${COMPONENT_LIB} PUBLIC SUPLA_DISABLE_LOGS)
  #add_definitions( -DSUPLA_DISABLE_LOGS )
endif()

