file(GLOB COMMON_SRCS "common/*.cpp")
file(GLOB HARDWARE_SRCS "hardware/*.cpp")
file(GLOB IO_SRCS "io/*.cpp")
file(GLOB NETWORK_SRCS "network/*.cpp")
file(GLOB DRIVERS_SRCS "drivers/*.c")

idf_component_register(SRCS "main.cpp" ${HARDWARE_SRCS} ${COMMON_SRCS} ${IO_SRCS} ${NETWORK_SRCS} ${DRIVERS_SRCS}
                    INCLUDE_DIRS "." REQUIRES esp_driver_gpio efuse esp_tinyusb nvs_flash esp_wifi led_strip esp_http_client app_update esp_driver_ledc)