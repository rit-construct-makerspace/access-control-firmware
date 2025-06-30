#include "LEDControl.hpp"

#include <chrono>
#include <thread>
#include <array>

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "common/pins.hpp"
#include "neopixel.hpp"
#include "esp_log.h"

using LEDState = std::array<espp::Rgb, 4>;

struct LEDAnimation {
    uint8_t length;
    std::array<LEDState, 16> frames;
};

TaskHandle_t led_thread;

#define LED_TASK_STACK_SIZE 4000

static LEDDisplayState display_state = LEDDisplayState::IDLE;
static SemaphoreHandle_t state_mutex;

const char * TAG = "led";

static const espp::Rgb WHITE(15, 15, 15);
static const espp::Rgb OFF(0, 0, 0);
static const espp::Rgb RED(15, 0, 0);
static const espp::Rgb RED_DIM = (RED + OFF) + OFF;
static const espp::Rgb GREEN(0, 15, 0);
static const espp::Rgb GREEN_DIM = (GREEN + OFF) + OFF;
static const espp::Rgb BLUE(0, 0, 15);
static const espp::Rgb YELLOW = RED + GREEN;
static const espp::Rgb ORANGE(15, 10, 0);

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

void advance_frame(LEDAnimation animation, espp::Neopixel &strip, uint8_t &current_frame) {
    if (current_frame + 1 >= animation.length) {
        current_frame = 0;
    } else {
        current_frame++;
    }

    for (int led = 0; led < strip.num_leds(); led++) {
        strip.set_color(animation.frames[current_frame][led], led);
    }
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
    espp::Neopixel strip({
        .data_gpio = LED_PIN,
        .num_leds = 4
    });

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

        if (!network_good && (current_frame % 2 == 0)) {
            strip.set_color(WHITE, 0);
            strip.set_color(WHITE, 3);
        }

        strip.show();
        vTaskDelay(pdMS_TO_TICKS(400));
    };
};

int led_init() {
    state_mutex = xSemaphoreCreateMutex();
    
    if (state_mutex == NULL) {
        // TODO: Fault here
        ESP_LOGE(TAG, "NO STATE MUTEX");
        return 1;
    }

    xTaskCreate(led_thread_fn, "led", LED_TASK_STACK_SIZE, NULL, 0,
                &led_thread);
    return 0;
};