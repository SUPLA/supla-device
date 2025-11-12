
idf_component_register(SRCS "esp_spi_driver.cpp"
                    INCLUDE_DIRS "."
                    REQUIRES
                      esp_driver_spi
                    PRIV_REQUIRES
                      supla-device
                    )

