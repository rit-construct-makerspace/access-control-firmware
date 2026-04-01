#include "hardware/hardware.hpp"
#include "hardware/storage.hpp"
#include "hardware/usb.hpp"
#include "io/IO.hpp"
#include "network/network.hpp"

#include <stdio.h>

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
    // USB::init();
    Storage::init();
    IO::init();
    Network::init();
}