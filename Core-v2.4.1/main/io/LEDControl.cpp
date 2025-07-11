#include "LEDControl.hpp"

#include "network/network.hpp"
#include <array>
#include <chrono>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <thread>

#include "common/hardware.hpp"
#include "common/pins.hpp"
#include "common/types.hpp"
#include "esp_log.h"
#include "io/LEDAnimations.hpp"
#include "led_strip.h"

TaskHandle_t led_thread;

#define LED_TASK_STACK_SIZE 3000
#define NUM_LEDS 4

static Animation::Animation current_animation = Animation::STARTUP;
static SemaphoreHandle_t animation_mutex;

static const char* TAG = "led";

led_strip_handle_t configure_led(void) {
    led_color_component_format_t strip_color_format;
    HardwareEdition edition = Hardware::get_edition();
    switch (edition) {
        case HardwareEdition::LITE:
            strip_color_format = LED_STRIP_COLOR_COMPONENT_FMT_RGB;
            break;
        case HardwareEdition::STANDARD:
            strip_color_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB;
            break;
    }
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN,                    // The GPIO that connected to the LED strip's data line
        .max_leds = 4,                                // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,                // LED strip model
        .color_component_format = strip_color_format, // The color order of the strip: RGB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }};

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,    // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency
        .mem_block_symbols = 0,            // the memory block size used by the RMT channel
        .flags = {
            .with_dma = 0, // Using DMA can improve performance when driving more LEDs
        }};

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void advance_frame(Animation::Animation animation, led_strip_handle_t& strip, uint8_t& current_frame) {
    if (current_frame + 1 >= animation.length) {
        current_frame = 0;
    } else {
        current_frame++;
    }

    for (int i = 0; i < 4; i++) {
        auto [r, g, b] = animation.frames[current_frame][i];
        led_strip_set_pixel(strip, i, r, g, b);
    }
}

bool LED::set_animation(Animation::Animation animation) {
    if (xSemaphoreTake(animation_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        current_animation = animation;
        xSemaphoreGive(animation_mutex);
        return true;
    } else {
        return false;
    }
};

// Returns true if current_state was updated, false otherwise
bool get_animation(Animation::Animation& next_animation) {
    if (xSemaphoreTake(animation_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (current_animation == next_animation) {
            xSemaphoreGive(animation_mutex);
            return false;
        }
        next_animation = current_animation;
        xSemaphoreGive(animation_mutex);
        return true;
    } else {
        return false;
    }
};

void led_thread_fn(void*) {
    led_strip_handle_t strip = configure_led();
    if (strip == NULL) {
        ESP_LOGI(TAG, "Failed to intialize LEDs");
        // TODO: Crash out
    }

    Animation::Animation thread_animation = Animation::STARTUP;
    bool network_good = false;
    uint8_t current_frame = 0;

    while (true) {
        if (get_animation(thread_animation)) {
            current_frame = 0;
        }

        advance_frame(thread_animation, strip, current_frame);

        network_good = Network::is_online();

        if (!network_good && (current_frame % 2 == 0)) {
            led_strip_set_pixel(strip, 0, Animation::WHITE[0], Animation::WHITE[0], Animation::WHITE[0]);
            led_strip_set_pixel(strip, 0, Animation::WHITE[0], Animation::WHITE[0], Animation::WHITE[0]);
        }

        led_strip_refresh(strip);
        vTaskDelay(pdMS_TO_TICKS(350));
    };
};

int LED::init() {
    animation_mutex = xSemaphoreCreateMutex();

    if (animation_mutex == NULL) {
        // TODO: Crash here
        return 1;
    }

    xTaskCreate(led_thread_fn, "led", CONFIG_LED_TASK_STACK_SIZE, NULL, 0, &led_thread);
    return 0;
};