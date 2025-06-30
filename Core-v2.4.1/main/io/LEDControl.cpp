#include "LEDControl.hpp"

#include <chrono>
#include <thread>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "common/pins.hpp"
#include "neopixel.h"
#include "esp_log.h"

using LEDState = std::array<uint32_t, 4>;

struct LEDAnimation {
    uint8_t length;
    std::array<LEDState, 16> frames;
};

TaskHandle_t led_thread;

#define LED_TASK_STACK_SIZE 4000
#define NUM_LEDS 4

static LEDDisplayState display_state;
static SemaphoreHandle_t state_mutex;

const char * TAG = "led";

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

    neopixel_SetPixel(strip, pixel, 4);
}

bool set_led_state(LEDDisplayState state) {
    if (xSemaphoreTake(state_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        display_state = state;
        xSemaphoreGive(state_mutex);
        return true;
    } else {
        return false;
    }
};

// Returns true if current_state was updated, false otherwise
bool get_led_state(LEDDisplayState &current_state) {
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
    return false;
};

void led_thread_fn(void *) {
    tNeopixelContext strip = neopixel_Init(NUM_LEDS, LED_PIN);
    if (strip == NULL) {
        ESP_LOGI(TAG, "Failed to intialize LEDs");
        // TODO: Crash out
    }

    LEDDisplayState loop_state = LEDDisplayState::STARTUP;
    bool network_good = get_network_state();
    uint8_t current_frame = 0;

    while (true) {

        if (get_led_state(loop_state)) {
            current_frame = 0;
        }
        network_good = get_network_state();

        switch (loop_state) {
            case LEDDisplayState::IDLE:
                advance_frame(IDLE_ANIMATION, strip, current_frame);
                break;
            case LEDDisplayState::ALWAYS_ON:
                advance_frame(ALWAYS_ON_ANIMATION, strip, current_frame);
                break;
            case LEDDisplayState::UNLOCKED:
                advance_frame(UNLOCKED_ANIMATION, strip, current_frame);
                break;
            case LEDDisplayState::LOCKOUT:
                advance_frame(LOCK_OUT_ANIMATION, strip, current_frame);
                break;
            case LEDDisplayState::DENIED:
                advance_frame(DENIED_ANIMATION, strip, current_frame);
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

        std::this_thread::sleep_for(std::chrono::milliseconds{400});
    };
};

int led_init() {
    state_mutex = xSemaphoreCreateMutex();
    
    if (state_mutex == NULL) {
        // TODO: Fault here
        return 1;
    }

    xTaskCreate(led_thread_fn, "led", LED_TASK_STACK_SIZE, NULL, 0, &led_thread);
    return 0;
};