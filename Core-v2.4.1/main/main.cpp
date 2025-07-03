#include "io/IO.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"

#include "esp_log.h"
void set_log_levels(){
    // esp_log_level_set("*", ESP_LOG_NONE); 
    // ESP_LOGI("lhuinsdvv", "Logs turned off");
}

extern "C" void app_main(void) {
    set_log_levels();
    USB::init();
    IO::init();
    Network::init();
}
