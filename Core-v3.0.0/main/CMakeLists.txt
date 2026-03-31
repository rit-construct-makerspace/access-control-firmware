file(GLOB COMMON_SRCS "common/*.cpp")
file(GLOB HARDWARE_SRCS "hardware/*.cpp")

idf_component_register(SRCS "main.c" ${HARDWARE_SRCS} ${COMMON_SRCS}
                    INCLUDE_DIRS "." REQUIRES esp_driver_gpio efuse)