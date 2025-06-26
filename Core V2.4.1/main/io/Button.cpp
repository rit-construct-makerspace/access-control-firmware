#include "Button.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <thread>
#include <chrono>

#include "common/pins.hpp"
#include "common/types.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "io/IO.hpp"

#define BUTTON_TASK_STACK_SIZE 2000
TaskHandle_t button_thread;

const char * TAG = "Button";

void button_thread_fn(void *) {
    int iterations_held;
    int restart_threshold = 60; // 3 seconds
    int status;
    while (true) {

        status = gpio_get_level(BUTTON_PIN);

        if (status == 1) {
            iterations_held++;
            if (iterations_held > restart_threshold) {
                IO::send_event({
                    .type = IOEventType::BUTTON_PRESSED,
                    .button = {
                        .type = ButtonEventType::HELD,
                    },
                });
            }
        } else {
            if (iterations_held > restart_threshold) {
                IO::send_event({
                    .type = IOEventType::BUTTON_PRESSED,
                    .button = {
                        .type = ButtonEventType::RELEASED,
                    },
                });
            } else if (iterations_held > 0) {
                IO::send_event({
                    .type = IOEventType::BUTTON_PRESSED,
                    .button = {
                        .type = ButtonEventType::CLICK,
                    },
                });
            }
        }

        std::this_thread::sleep_for(std::chrono::milliseconds{50});
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