idf_component_register(SRCS "main.cpp" "io/LEDControl.cpp" "network/network.cpp" "network/logging.cpp" "network/storage.cpp" "common/types.cpp"
                    INCLUDE_DIRS "." REQUIRES neopixel esp_event esp_wifi PRIV_REQUIRES nvs_flash json)
