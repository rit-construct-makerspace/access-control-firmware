#include "LEDControl.hpp"

#include <chrono>
#include <thread>
#include <array>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>

#include "common/pins.hpp"

#include "esp_log.h"

using LEDState = std::array<uint32_t, 4>;

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

static const uint32_t WHITE = NP_RGB(15, 15, 15);
static const uint32_t OFF = NP_RGB(0, 0, 0);
static const uint32_t RED = NP_RGB(15, 0, 0);
static const uint32_t RED_DIM = NP_RGB(5, 0, 0);
static const uint32_t GREEN = NP_RGB(0, 15, 0);
static const uint32_t GREEN_DIM = NP_RGB(0, 5, 0);
static const uint32_t BLUE = NP_RGB(0, 0, 15);
static const uint32_t ORANGE = NP_RGB(15, 10, 0);

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

void advance_frame(LEDAnimation animation, tNeopixelContext &strip, uint8_t &current_frame) {
    if (current_frame + 1 >= animation.length) {
        current_frame = 0;
    } else {
        current_frame++;
    }

    tNeopixel pixel[] = {
        { 0, animation.frames[current_frame][0]},
        { 1, animation.frames[current_frame][1]},
        { 2, animation.frames[current_frame][2]},
        { 3, animation.frames[current_frame][3]},
    };

    //neopixel_SetPixel(strip, pixel, 4);
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
    tNeopixelContext strip = neopixel_Init(NUM_LEDS, LED_PIN);
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

        tNeopixel network_pixels[] = {
            {0, WHITE},
            {3, WHITE},
        };

        if (!network_good && (current_frame % 2 == 0)) {
            neopixel_SetPixel(strip, network_pixels, 2);
        }
        vTaskDelay(pdMS_TO_TICKS(400));
    };
};

int LED::init() {
    state_mutex = xSemaphoreCreateMutex();
    
    if (state_mutex == NULL) {
        // TODO: Crash here
        return 1;
    }

    xTaskCreate(led_thread_fn, "led", LED_TASK_STACK_SIZE, NULL, 0, &led_thread);
    return 0;
};