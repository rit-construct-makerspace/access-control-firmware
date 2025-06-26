#include "network.hpp"
#include "esp_log.h"
#include "storage.hpp"

#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <chrono>
#include <thread>

static const char *TAG = "network";

int debug_log_init();

int network_thread_fn(void*) {
    while (true) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    return 0;
}

namespace Network {
    int init() {
        debug_log_init();
        Storage::init();

        return 0;
    }

    void report_state_transition(IOState from, IOState to) {
        ESP_LOGI(TAG, "Transition %s -> %s", io_state_to_string(from).c_str(),
                 io_state_to_string(to).c_str());
    }

    bool is_online() { return true; }
} // namespace Network

namespace bad {}
