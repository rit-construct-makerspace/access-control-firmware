idf_component_register(SRCS "main.cpp" "network/network.cpp" "network/logging.cpp"
                    INCLUDE_DIRS "." REQUIRES neopixel esp_event esp_wifi PRIV_REQUIRES nvs_flash json_generator)
