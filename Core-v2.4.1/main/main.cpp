#include "common/hardware.hpp"
#include "common/pins.hpp"
#include "common/types.hpp"
#include "esp_heap_trace.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "io/IO.hpp"
#include "network/http_manager.hpp"
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
    // Storage::set_server("make.rit.edu");
    // Storage::set_key(
        // "29c784ab67aaf05b986b4362d28a5fd58df9d48947d7c64f5c1ab1c769df3a39d4f2b00cab2e096917407c4c71c044e2");
    Storage::set_network_password({0});
    Storage::set_network_ssid({'R', 'I', 'T', '-', 'W', 'i', 'F', 'i', 0x0});
    IO::init();
    Network::init();
    ESP_LOGI("main", "Free heap %lu", esp_get_free_heap_size());
    ESP_LOGI("main", "Free heap %lu", esp_get_free_heap_size());

    // vTaskDelay(500);
    // HTTPManager::init("make.rit.edu");
    // for (int i = 0; i < 500; i++) {
    //     ESP_LOGI("main", "Free heap %lu", esp_get_free_heap_size());
    //     vTaskDelay(100);
    // }
    // // Network::send_internal_event({
    // // .type = Network::InternalEventType::OtaUpdate,
    // // .ota_tag = {'a', 'b', 'c', 'd', 0},
    // // });
}
