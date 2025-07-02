#include "LEDControl.hpp"

#include <chrono>
#include <thread>
#include <array>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "common/pins.hpp"
#include "esp_log.h"
#include "led_strip.h"

using RGB = std::array<uint32_t, 3>;

using LEDState = std::array<RGB, 4>;

struct LEDAnimation {
    uint8_t length;
    std::array<LEDState, 16> frames;
};

TaskHandle_t led_thread;

#define LED_TASK_STACK_SIZE 4000
#define NUM_LEDS 4

static LED::DisplayState display_state = LED::DisplayState::STARTUP;
static SemaphoreHandle_t state_mutex;

static const char * TAG = "led";

static const RGB WHITE = {15, 15, 15};
static const RGB OFF = {0, 0, 0};
static const RGB RED = {15, 0, 0};
static const RGB RED_DIM = {5, 0, 0};
static const RGB GREEN = {0, 15, 0};
static const RGB GREEN_DIM = {0, 5, 0};
static const RGB BLUE = {0, 0, 15};
static const RGB ORANGE = {15, 10, 0};

const LEDAnimation IDLE_ANIMATION {
    .length = 2,
    .frames = {
        LEDState{ORANGE, ORANGE, ORANGE, ORANGE},
        LEDState{ORANGE, ORANGE, ORANGE, ORANGE},
    }
};

const LEDAnimation LOCK_OUT_ANIMATION {
    .length = 2,
    .frames = {
        LEDState{RED, RED, RED, RED},
        LEDState{RED, RED, RED, RED},
    }
};

const LEDAnimation UNLOCKED_ANIMATION {
    .length = 2,
    .frames = {
        LEDState{GREEN, GREEN, GREEN, GREEN},
        LEDState{GREEN, GREEN, GREEN, GREEN},
    }
};

const LEDAnimation ALWAYS_ON_ANIMATION {
    .length = 8,
    .frames = {
        LEDState {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
        LEDState {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
        LEDState {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
        LEDState {GREEN, GREEN_DIM, GREEN, GREEN_DIM},
        LEDState {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
        LEDState {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
        LEDState {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
        LEDState {GREEN_DIM, GREEN, GREEN_DIM, GREEN},
    }
};

const LEDAnimation DENIED_ANIMATION {
    .length = 2,
    .frames = {
        LEDState {RED, RED, RED, RED},
        LEDState {RED_DIM, RED_DIM, RED_DIM, RED_DIM},
    }
};

const LEDAnimation STARTUP_ANIMATION {
    .length = 3,
    .frames = {
        LEDState {RED, RED, RED, RED},
        LEDState {GREEN, GREEN, GREEN, GREEN},
        LEDState {BLUE, BLUE, BLUE, BLUE},
    }
};

const LEDAnimation IDLE_WAITING_ANIMATION {
    .length = 2,
    .frames = {
        LEDState {OFF, ORANGE, ORANGE, OFF},
        LEDState {OFF, OFF, OFF, OFF},
    }
};

const LEDAnimation ALWAYS_ON_WAITING_ANIMATION {
    .length = 2,
    .frames = {
        LEDState {OFF, GREEN, GREEN, OFF},
        LEDState {OFF, OFF, OFF, OFF},
    }
};

const LEDAnimation LOCKOUT_WAITING_ANIMATION {
    .length = 2,
    .frames = {
        LEDState {OFF, RED, RED, OFF},
        LEDState {OFF, OFF, OFF, OFF},
    }
};

const LEDAnimation RESTART_ANIMATION {
    .length = 3,
    .frames = {
        LEDState {RED, GREEN, BLUE, RED},
        LEDState {BLUE, RED, GREEN, BLUE},
        LEDState {GREEN, BLUE, RED, GREEN},
    }
};

led_strip_handle_t configure_led(void) {
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_PIN, // The GPIO that connected to the LED strip's data line
        .max_leds = 4,      // The number of LEDs in the strip,
        .led_model = LED_MODEL_WS2812,        // LED strip model
        .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB, // The color order of the strip: RGB
        .flags = {
            .invert_out = false, // don't invert the output signal
        }
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // RMT counter clock frequency
        .mem_block_symbols = 0, // the memory block size used by the RMT channel
        .flags = {
            .with_dma = 0,     // Using DMA can improve performance when driving more LEDs
        }
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

void advance_frame(LEDAnimation animation, led_strip_handle_t &strip, uint8_t &current_frame) {
    if (current_frame + 1 >= animation.length) {
        current_frame = 0;
    } else {
        current_frame++;
    }

    for (int i = 0; i < 4; i++) {
        led_strip_set_pixel(strip, i, animation.frames[current_frame][i][0], animation.frames[current_frame][i][1], animation.frames[current_frame][i][2]);
    }
}

bool LED::set_state(LED::DisplayState state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        display_state = state;
        xSemaphoreGive(state_mutex);
        return true;
    } else {
        return false;
    }
};

// Returns true if current_state was updated, false otherwise
bool get_state(LED::DisplayState &current_state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        if (current_state == display_state) {
            xSemaphoreGive(state_mutex);
            return false;
        }
        current_state = display_state;
        xSemaphoreGive(state_mutex);
        return true;
    } else {
        return false;
    }
};

bool get_network_state() {
    // TODO: Ask the network task
    return true;
};

void led_thread_fn(void *) {
    led_strip_handle_t strip = configure_led();
    if (strip == NULL) {
        ESP_LOGI(TAG, "Failed to intialize LEDs");
        // TODO: Crash out
    }

    LED::DisplayState loop_state = LED::DisplayState::STARTUP;
    bool network_good = get_network_state();
    uint8_t current_frame = 0;

    while (true) {
        if (get_state(loop_state)) {
            current_frame = 0;
        }
        network_good = get_network_state();

        switch (loop_state) {
            case LED::DisplayState::IDLE:
                advance_frame(IDLE_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::ALWAYS_ON:
                advance_frame(ALWAYS_ON_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::UNLOCKED:
                advance_frame(UNLOCKED_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::LOCKOUT:
                advance_frame(LOCK_OUT_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::DENIED:
                advance_frame(DENIED_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::STARTUP:
                advance_frame(STARTUP_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::IDLE_WAITING:
                advance_frame(IDLE_WAITING_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::ALWAYS_ON_WAITING:
                advance_frame(ALWAYS_ON_WAITING_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::LOCKOUT_WAITING:
                advance_frame(LOCKOUT_WAITING_ANIMATION, strip, current_frame);
                break;
            case LED::DisplayState::RESTART:
                advance_frame(RESTART_ANIMATION, strip, current_frame);
                break;
            default:
                break;
        }

        if (!network_good && (current_frame % 2 == 0)) {
            led_strip_set_pixel(strip, 0, WHITE[0], WHITE[0], WHITE[0]);
            led_strip_set_pixel(strip, 3, WHITE[0], WHITE[0], WHITE[0]);
        }

        led_strip_refresh(strip);

        vTaskDelay(pdMS_TO_TICKS(400));
    };
};

int LED::init() {
    state_mutex = xSemaphoreCreateMutex();
    
    if (state_mutex == NULL) {
        // TODO: Crash here
        return 1;
    }

    xTaskCreate(led_thread_fn, "led", LED_TASK_STACK_SIZE, NULL, 0,
                &led_thread);
    return 0;
};