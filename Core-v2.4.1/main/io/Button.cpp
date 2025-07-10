#include "Button.hpp"

#include <chrono>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <thread>

#include "common/pins.hpp"
#include "common/types.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "io/IO.hpp"

#define BUTTON_TASK_STACK_SIZE 2000
TaskHandle_t button_thread;

static const char* TAG = "Button";

void button_thread_fn(void*) {
    int iterations_held = 0;
    int restart_threshold = 60; // 3 seconds
    int status;
    while (true) {

        status = !gpio_get_level(BUTTON_PIN);

        if (status == 1) {
            iterations_held++;
            if (iterations_held > restart_threshold) {
                IO::send_event({
                    .type = IOEventType::BUTTON_PRESSED,
                    .button =
                        {
                            .type = ButtonEventType::HELD,
                        },
                });
            }
        } else {
            if (iterations_held > restart_threshold) {
                IO::send_event({
                    .type = IOEventType::BUTTON_PRESSED,
                    .button =
                        {
                            .type = ButtonEventType::RELEASED,
                        },
                });
                iterations_held = 0;
            } else if (iterations_held > 0) {
                IO::send_event({
                    .type = IOEventType::BUTTON_PRESSED,
                    .button =
                        {
                            .type = ButtonEventType::CLICK,
                        },
                });
                iterations_held = 0;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    };
}

int Button::init() {
    if (gpio_input_enable(BUTTON_PIN) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to configure button pin");
        // TODO: Crash
    }

    xTaskCreate(button_thread_fn, "button", BUTTON_TASK_STACK_SIZE, NULL, 0, &button_thread);

    return 0;
}