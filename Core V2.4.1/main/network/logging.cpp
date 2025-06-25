
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "websocket.hpp"

#include "sdkconfig.h"

#include "common/types.hpp"

QueueHandle_t debug_log_queue;
TaskHandle_t network_thread;

void debug_log(const LogMessage &msg) {
  LogMessage *p_msg = new LogMessage(msg);
  xQueueSend(debug_log_queue, &p_msg, 100);
}

static const char *TAG = "log";

void debug_log_thread_fn(void *) {
  while (true) {
    LogMessage *msg = 0;
    if (xQueueReceive(debug_log_queue, &msg, portMAX_DELAY)) {
      bool isOffline = false;
      if (isOffline) {
        ESP_LOGI(TAG, "logging to flash");
      } else {
        ESP_LOGI(TAG, "logging to network");
      }
      ESP_LOGI(TAG, "%s: %s",
               (msg->type == LogMessageType::DEBUG
                    ? "DEBUG"
                    : (msg->type == LogMessageType::ERROR ? "ERROR" : "NORML")),
               msg->message.c_str());

      delete msg;
    }
  }
}

#define LOGGING_STACK_SIZE 2048

int debug_log_init() {
  debug_log_queue =
      xQueueCreate(CONFIG_DEBUG_LOG_QUEUE_SIZE, sizeof(LogMessage *));

  xTaskCreate(debug_log_thread_fn, "network", LOGGING_STACK_SIZE, NULL, 0,
              &network_thread);
  return 0;
}
