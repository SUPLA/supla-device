
idf_component_register(SRCS "esp_uart_driver.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES
                      supla-device
                    REQUIRES
                      driver
                    )

