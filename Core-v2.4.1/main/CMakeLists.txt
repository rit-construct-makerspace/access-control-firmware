file(GLOB IO_SRCS "io/*.cpp")
file(GLOB COMMON_SRCS "common/*.cpp")
file(GLOB NET_SRCS "network/*.cpp")
file(GLOB DRIVER_SRCS "drivers/*.c")

idf_component_register(SRCS "main.cpp" ${IO_SRCS} ${COMMON_SRCS} ${NET_SRCS} ${DRIVER_SRCS}
                        INCLUDE_DIRS "." REQUIRES led_strip esp_wifi nvs_flash json esp_driver_gpio lwip esp_http_client esp_websocket_client esp_driver_ledc onewire_bus ds18b20 efuse app_update)