#include "websocket.hpp"
#include "cJSON.h"
#include "esp_log.h"

static const char *TAG = "websocket";

void submit_log_data(cJSON *json) {

  ESP_LOGI(TAG, "Sending websocket message");
}