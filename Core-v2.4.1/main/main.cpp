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
    IO::init();
    Network::init();

    vTaskDelay(500);
    Network::send_internal_event({
        .type = Network::InternalEventType::OtaUpdate,
        .ota_tag = {'a', 'b', 'c', 'd', 0},
    });
}
