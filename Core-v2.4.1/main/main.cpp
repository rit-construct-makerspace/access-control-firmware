#include "io/IO.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"
#include "common/types.hpp"
#include "driver/gpio.h"
#include "common/pins.hpp"

HardwareEdition edition;

HardwareEdition get_hardware_edition() {
    return edition;
};

#include "esp_log.h"
void set_log_levels(){
    esp_log_level_set("tusb_desc", ESP_LOG_NONE); 
    esp_log_level_set("wifi_init", ESP_LOG_NONE); 
    esp_log_level_set("phy_init", ESP_LOG_NONE); 
    esp_log_level_set("wifi", ESP_LOG_NONE);

    
    // ESP_LOGI("lhuinsdvv", "Logs turned off");
}

extern "C" void app_main(void) {
    set_log_levels();

    gpio_input_enable(MODE);

    edition = (HardwareEdition) gpio_get_level(MODE);

    USB::init();
    IO::init();
    Network::init();
}
