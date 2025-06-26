#include "network.hpp"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "sdkconfig.h"

#include <chrono>
#include <thread>

static const char *TAG = "network";

int debug_log_init();
int network_init() {
  debug_log_init();

  return 0;
}