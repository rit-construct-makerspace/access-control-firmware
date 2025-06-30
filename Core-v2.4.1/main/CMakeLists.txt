file(GLOB IO_SRCS "io/*.cpp")
file(GLOB COMMON_SRCS "common/*.cpp")
file(GLOB NET_SRCS "network/*.cpp")

idf_component_register(SRCS "main.cpp" ${IO_SRCS} ${COMMON_SRCS} ${NET_SRCS}
                    INCLUDE_DIRS "." REQUIRES neopixel esp_wifi nvs_flash json)
