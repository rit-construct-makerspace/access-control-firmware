file(GLOB IO_SRCS "io/*.cpp")
file(GLOB COMMON_SRCS "common/*.cpp")
file(GLOB NET_SRCS "network/*.cpp")
file(GLOB DRIVER_SRCS "drivers/*.c")

idf_component_register(SRCS "main.cpp" ${IO_SRCS} ${COMMON_SRCS} ${NET_SRCS} ${DRIVER_SRCS}
                        INCLUDE_DIRS "." REQUIRES esp_wifi nvs_flash json esp_driver_i2s)