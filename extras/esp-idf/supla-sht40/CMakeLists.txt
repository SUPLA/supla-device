
idf_component_register(SRCS "esp_sht40.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES
                      driver
                    PRIV_REQUIRES
                      supla-device
                    )

