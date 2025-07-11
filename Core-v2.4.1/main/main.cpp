#include "common/hardware.hpp"
#include "common/pins.hpp"
#include "common/types.hpp"
#include "esp_heap_trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io/IO.hpp"
#include "network/network.hpp"
#include "network/storage.hpp"
#include "network/usb.hpp"

#include "esp_log.h"
void set_log_levels() {
    esp_log_level_set("tusb_desc", ESP_LOG_NONE);
    esp_log_level_set("wifi_init", ESP_LOG_NONE);
    esp_log_level_set("phy_init", ESP_LOG_NONE);
    esp_log_level_set("wifi", ESP_LOG_NONE);
}

extern "C" void app_main(void) {
    set_log_levels();
    Hardware::init();
    USB::init();
    Storage::init();
    Storage::set_key("6de6833db7f7d4050687c83667c0a64af9b44f83d0b187ab35f35d0620e05b31e59a255ffb26fe4d9376d825430aad7c");
    IO::init();
    Network::init();
}
