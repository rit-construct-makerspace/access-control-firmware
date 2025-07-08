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
    esp_log_level_set("lwIP", ESP_LOG_DEBUG);

    // ESP_LOGI("lhuinsdvv", "Logs turned off");
}
// io  -19240
// net -13030
// 
extern "C" void app_main(void) {
    set_log_levels();
    Hardware::identify();

    USB::init();
    uint32_t size = esp_get_free_heap_size();
    ESP_LOGI("main", "Heap size post usb: %lu", size);

    ESP_LOGI("main", "SN is %s", Hardware::get_serial_number());
    IO::init();
    size = esp_get_free_heap_size();
    ESP_LOGI("main", "Heap size post io: %lu", size);

    Network::init();
    size = esp_get_free_heap_size();
    ESP_LOGI("main", "Heap size post network: %lu", size);

    // ESP_ERROR_CHECK(heap_trace_init_standalone(trace_record, NUM_RECORDS));
    //

    // ESP_ERROR_CHECK(heap_trace_start(HEAP_TRACE_LEAKS));

    int i = 0;
    while (1) {
        i++;

        vTaskDelay(100);
        uint32_t size = esp_get_free_heap_size();
        ESP_LOGI("main", "Heap size: %lu", size);
    }
}
