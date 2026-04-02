idf_component_register(SRCS "ledc_io.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES
                      esp_driver_ledc
                    PRIV_REQUIRES
                      supla-device
                    )
