#include "io/IO.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"
#include "common/types.hpp"
#include "common/pins.hpp"
#include "network/storage.hpp"

#include "esp_log.h"
void set_log_levels(){
    esp_log_level_set("tusb_desc", ESP_LOG_NONE); 
    esp_log_level_set("wifi_init", ESP_LOG_NONE); 
    esp_log_level_set("phy_init", ESP_LOG_NONE); 
    esp_log_level_set("wifi", ESP_LOG_NONE);
    esp_log_level_set("lwIP", ESP_LOG_DEBUG);

    
    // ESP_LOGI("lhuinsdvv", "Logs turned off");
}

extern "C" void app_main(void) {
    set_log_levels();
    Hardware::identify();

    ESP_LOGI("main", "SN is %s", Hardware::get_serial_number());


    USB::init();
    IO::init();
    Network::init();
}
