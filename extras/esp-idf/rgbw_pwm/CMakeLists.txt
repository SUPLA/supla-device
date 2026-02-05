idf_component_register(SRCS "rgbw_pwm.cpp" "dimmer_pwm.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES
                      esp_driver_ledc
                    PRIV_REQUIRES
                      supla-device
                    )

