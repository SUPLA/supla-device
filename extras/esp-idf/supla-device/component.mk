COMPONENT_ADD_INCLUDEDIRS := ../../../src \
                             ../../porting/esp-idf

COMPONENT_OBJS := \
  ../../../src/supla-common/lck.o \
  ../../../src/supla-common/log.o \
  ../../../src/supla-common/proto.o \
  ../../../src/supla-common/srpc.o \
	\
  ../../../src/SuplaDevice.o \
  ../../../src/supla/action_handler.o \
  ../../../src/supla/at_channel.o \
  ../../../src/supla/channels/channel.o \
	../../../src/supla/channels/binary_sensor_channel.o \
  ../../../src/supla/channel_element.o \
  ../../../src/supla/element_with_channel_actions.o \
  ../../../src/supla/channels/channel_extended.o \
  ../../../src/supla/correction.o \
  ../../../src/supla/element.o \
  ../../../src/supla/io.o \
  ../../../src/supla/local_action.o \
  ../../../src/supla/log_wrapper.o \
  ../../../src/supla/time.o \
  ../../../src/supla/timer.o \
  ../../../src/supla/tools.o \
  ../../../src/supla/uptime.o \
  ../../../src/supla/mutex.o \
  ../../../src/supla/auto_lock.o \
  ../../../src/supla/sha256.o \
  ../../../src/supla/rsa_verificator.o \
  ../../../src/supla/crc8.o \
  ../../../src/supla/crc16.o \
	\
  ../../../src/supla/storage/storage.o \
  ../../../src/supla/storage/config.o \
	\
  ../../../src/supla/network/network.o \
  ../../../src/supla/network/netif_wifi.o \
  ../../../src/supla/network/web_server.o \
  ../../../src/supla/network/web_sender.o \
  ../../../src/supla/network/html_generator.o \
  ../../../src/supla/network/html_element.o \
  ../../../src/supla/network/html/device_info.o \
  ../../../src/supla/network/html/wifi_parameters.o \
  ../../../src/supla/network/html/protocol_parameters.o \
  ../../../src/supla/network/html/status_led_parameters.o \
  ../../../src/supla/network/html/custom_sw_update.o \
  ../../../src/supla/network/html/sw_update_beta.o \
  ../../../src/supla/network/html/sw_update.o \
  ../../../src/supla/network/html/button_multiclick_parameters.o \
  ../../../src/supla/network/html/custom_parameter.o \
  ../../../src/supla/network/html/channel_correction.o \
  ../../../src/supla/network/html/hide_show_container.o \
  ../../../src/supla/network/html/div.o \
  ../../../src/supla/network/html/h2_tag.o \
  ../../../src/supla/network/html/h3_tag.o \
  ../../../src/supla/network/html/button_action_trigger_config.o \
  ../../../src/supla/network/html/text_cmd_input_parameter.o \
  ../../../src/supla/network/html/select_cmd_input_parameter.o \
  ../../../src/supla/network/html/volume_parameters.o \
  ../../../src/supla/network/html/screen_delay_parameters.o \
  ../../../src/supla/network/html/screen_brightness_parameters.o \
  ../../../src/supla/network/client.o \
  ../../../src/supla/network/ip_address.o \
  \
  ../../../src/supla/protocol/protocol_layer.o \
  ../../../src/supla/protocol/mqtt.o \
  ../../../src/supla/protocol/mqtt_topic.o \
  ../../../src/supla/protocol/supla_srpc.o \
  \
  ../../../src/supla/clock/clock.o \
	\
  ../../../src/supla/control/action_trigger.o \
  ../../../src/supla/control/bistable_relay.o \
  ../../../src/supla/control/bistable_roller_shutter.o \
  ../../../src/supla/control/button.o \
  ../../../src/supla/control/dimmer_base.o \
  ../../../src/supla/control/dimmer_leds.o \
  ../../../src/supla/control/internal_pin_output.o \
  ../../../src/supla/control/light_relay.o \
  ../../../src/supla/control/pin_status_led.o \
  ../../../src/supla/control/relay.o \
  ../../../src/supla/control/rgb_base.o \
  ../../../src/supla/control/rgb_leds.o \
  ../../../src/supla/control/rgbw_base.o \
  ../../../src/supla/control/rgbw_leds.o \
  ../../../src/supla/control/roller_shutter.o \
  ../../../src/supla/control/sequence_button.o \
  ../../../src/supla/control/simple_button.o \
  ../../../src/supla/control/virtual_relay.o \
	\
  ../../../src/supla/condition.o \
  ../../../src/supla/condition_getter.o \
  ../../../src/supla/conditions/on_less.o \
  ../../../src/supla/conditions/on_less_eq.o \
  ../../../src/supla/conditions/on_greater.o \
  ../../../src/supla/conditions/on_greater_eq.o \
  ../../../src/supla/conditions/on_between.o \
  ../../../src/supla/conditions/on_between_eq.o \
  ../../../src/supla/conditions/on_equal.o \
  ../../../src/supla/conditions/on_invalid.o \
	\
  ../../../src/supla/device/status_led.o \
  ../../../src/supla/device/last_state_logger.o \
  ../../../src/supla/device/sw_update.o \
  ../../../src/supla/device/factory_test.o \
  ../../../src/supla/device/remote_device_config.o \
	\
  ../../../src/supla/sensor/binary.o \
	../../../src/supla/sensor/binary_base.o \
  ../../../src/supla/sensor/electricity_meter.o \
  ../../../src/supla/sensor/hygro_meter.o \
  ../../../src/supla/sensor/impulse_counter.o \
  ../../../src/supla/sensor/therm_hygro_meter.o \
  ../../../src/supla/sensor/therm_hygro_press_meter.o \
  ../../../src/supla/sensor/thermometer.o \
  ../../../src/supla/sensor/virtual_binary.o \
  ../../../src/supla/sensor/distance.o \
  ../../../src/supla/sensor/HC_SR04.o \
	\
  ../../../src/nettle/bignum.o \
  ../../../src/nettle/gmp-glue.o \
  ../../../src/nettle/mini-gmp.o \
  ../../../src/nettle/pkcs1-rsa-sha256.o \
  ../../../src/nettle/pkcs1.o \
  ../../../src/nettle/rsa-sha256-verify.o \
  ../../../src/nettle/rsa-verify.o \
  ../../../src/nettle/rsa.o \
  ../../../src/nettle/sha256-compress.o \
  ../../../src/nettle/sha256.o \
  ../../../src/nettle/write-be32.o \
	\
  ../../porting/esp-idf/esp_platform.o \
  ../../porting/esp-idf/esp_idf_wifi.o \
  ../../porting/esp-idf/esp_idf_gpio.o \
  ../../porting/esp-idf/spiffs_storage.o \
  ../../porting/esp-idf/nvs_config.o \
  ../../porting/esp-idf/esp_idf_web_server.o \
  ../../porting/esp-idf/esp_idf_mutex.o \
  ../../porting/esp-idf/esp_idf_ota.o \
  ../../porting/esp-idf/esp_idf_client.o \


COMPONENT_SRCDIRS := ../../../src/supla-common \
										 ../../../src/supla \
										 ../../../src/supla/conditions \
										 ../../../src/supla/channels \
										 ../../../src/supla/clock \
										 ../../../src/supla/control \
										 ../../../src/supla/device \
										 ../../../src/supla/sensor \
										 ../../../src/supla/storage \
										 ../../../src \
										 ../../../src/supla/network \
										 ../../../src/supla/network/html \
										 ../../../src/supla/protocol \
										 ../../porting/esp-idf \
										 ../../../src/nettle

CFLAGS += -DSUPLA_DEVICE -DSUPLA_DEVICE_ESP8266
CPPFLAGS += -DSUPLA_DEVICE -DSUPLA_DEVICE_ESP8266

COMPONENT_EMBED_TXTFILES := supla_org_cert.pem supla_3rd_cert.pem
