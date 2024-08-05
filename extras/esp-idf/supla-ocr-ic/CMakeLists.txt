
idf_component_register(SRCS "supla_ocr_ic.cpp"
                    INCLUDE_DIRS "."
                    PRIV_REQUIRES
                      supla-device
                      esp_psram
                    REQUIRES
                      esp32-camera
                    )

