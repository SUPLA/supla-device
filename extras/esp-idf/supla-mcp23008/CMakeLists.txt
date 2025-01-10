
idf_component_register(SRCS "supla_mcp23008.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES
                      supla-device
                      supla-i2c-driver
                    )

