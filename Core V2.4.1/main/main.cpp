/* WiFi station Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "nvs_flash.h"
#include <string.h>

#include "lwip/err.h"
#include "lwip/sys.h"

#include "network/network.hpp"

const char *TAG = "main";

extern "C" void app_main(void) {
  network_init();
  for (int i = 0; i < 100; i++) {
    ESP_LOGI(TAG, "Counting %d", i);
    debug_log({LogMessageType::DEBUG, "Something, man"});
    sleep(1);
  }
}
