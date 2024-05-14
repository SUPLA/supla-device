
idf_component_register(SRCS "esp_sht30.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES supla-device driver
                    REQUIRES supla-i2c-driver
                    )

