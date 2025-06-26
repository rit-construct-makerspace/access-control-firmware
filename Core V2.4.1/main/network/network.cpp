#include "network.hpp"
#include "esp_log.h"

#include "sdkconfig.h"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include <chrono>
#include <thread>

static const char *TAG = "network";

int debug_log_init();

namespace Network {
    int init() {
        debug_log_init();
        return 0;
    }

    void report_state_transition(IOState from, IOState to) {
        ESP_LOGI(TAG, "Transition %s -> %s", io_state_to_string(from).c_str(),
                 io_state_to_string(to).c_str());
    }

    bool is_online() { return true; }

} // namespace Network
