
idf_component_register(SRCS "esp_idf_lan8720.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES
                      supla-device
                    REQUIRES
                      esp_eth
                    )

