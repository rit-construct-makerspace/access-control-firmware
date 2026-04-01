file(GLOB COMMON_SRCS "common/*.cpp")
file(GLOB HARDWARE_SRCS "hardware/*.cpp")
file(GLOB IO_SRCS "io/*.cpp")

idf_component_register(SRCS "main.cpp" ${HARDWARE_SRCS} ${COMMON_SRCS} ${IO_SRCS}
                    INCLUDE_DIRS "." REQUIRES esp_driver_gpio efuse esp_tinyusb nvs_flash led_strip)