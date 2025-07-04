#include "IO.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>
#include <freertos/timers.h>

#include "esp_log.h"
#include "io/LEDControl.hpp"
#include "io/Button.hpp"
#include "io/CardReader.hpp"
#include "io/Buzzer.hpp"

static const char* TAG = "io";

QueueHandle_t event_queue;
TaskHandle_t io_thread;


#define IO_TASK_STACK_SIZE 4000

static IOState state = IOState::STARTUP;
static SemaphoreHandle_t animation_mutex;

bool IO::get_state(IOState &send_state) {
    if (xSemaphoreTake(animation_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        send_state = state;
        xSemaphoreGive(animation_mutex);
        return true;
    } else {
        return false;
    }
}

bool IO::send_event(IOEvent event) {
    return xQueueSend(event_queue, &(event), pdMS_TO_TICKS(100)) == pdTRUE;
}

bool set_state(IOState new_state) {
    if (xSemaphoreTake(animation_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        state = new_state;
        xSemaphoreGive(animation_mutex);
        return true;
    } else {
        return false;
    }
}

void go_to_state(IOState next_state) {
    IOState current_state;
    IO::get_state(current_state);

    if (next_state == IOState::RESTART) {
        LED::set_animation(Animation::RESTART_ANIMATION);
        return;
    }

    if (current_state == IOState::FAULT) {
        ESP_LOGE(TAG, "Attempted to go to %s from FAULT", io_state_to_string(next_state));
        return;
    }

    switch (next_state) {
        case IOState::IDLE:
            LED::set_animation(Animation::IDLE_ANIMATION);
            break;
        case IOState::UNLOCKED:
            Buzzer::send_effect(SoundEffect::ACCEPTED_EFFECT);
            LED::set_animation(Animation::UNLOCKED_ANIMATION);
            break;
        case IOState::ALWAYS_ON:
            LED::set_animation(Animation::ALWAYS_ON_ANIMATION);
            break;
        case IOState::LOCKOUT:
            Buzzer::send_effect(SoundEffect::LOCKOUT_EFFECT);
            LED::set_animation(Animation::LOCKOUT_ANIMATION);
            break;
        case IOState::NEXT_CARD:
            //LED::set_animation(Animation::NEXT_CARD_ANIMATION);
            break;
        case IOState::WELCOMING:
            //LED::set_animation(Animation::WELCOMING_ANIMATION);
            break;
        case IOState::WELCOMED:
            Buzzer::send_effect(SoundEffect::ACCEPTED_EFFECT);
            //LED::set_animation(Animation::WELCOMED_ANIMATION);
            break;
        case IOState::ALWAYS_ON_WAITING:
            LED::set_animation(Animation::ALWAYS_ON_WAITING_ANIMATION);
            break;
        case IOState::LOCKOUT_WAITING:
            LED::set_animation(Animation::LOCKOUT_WAITING_ANIMATION);
            break;
        case IOState::IDLE_WAITING:
            LED::set_animation(Animation::IDLE_WAITING_ANIMATION);
            break;
        case IOState::AWAIT_AUTH:
            //LED::set_animation(Animation::AWAIT_AUTH_ANIMATION);
            break;
        case IOState::DENIED:
            Buzzer::send_effect(SoundEffect::DENIED_EFFECT);
            LED::set_animation(Animation::DENIED_ANIMATION);
            break;
        case IOState::FAULT:
            Buzzer::send_effect(SoundEffect::FAULT_EFFECT);
            LED::set_animation(Animation::FAULT_ANIMATION);
            break;
        default:
            ESP_LOGI(TAG, "Attempted to go to an unkown state");
        return;
    }

    if (!set_state(next_state)) {
        ESP_LOGI(TAG, "Failed to update the stored state");
        // TODO: Crash
    }
}

IOState waiting_prior_state;
TimerHandle_t waiting_timer;

void waiting_timer_callback(TimerHandle_t timer) {
    go_to_state(waiting_prior_state);
};

void timer_refresh() {
    if (xTimerIsTimerActive(waiting_timer) == pdFALSE) {
        if (xTimerStart(waiting_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
            // TODO: Crash
        }
    } else {
        if (xTimerReset(waiting_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
            // TODO: Crash
        }
    }
}

void handle_button_clicked() {
    IOState current_state;
    if (!IO::get_state(current_state)) {
        ESP_LOGE(TAG, "Failed to get state");
        return;
    }

    switch (current_state) {
        case IOState::UNLOCKED:
        case IOState::AWAIT_AUTH:
        case IOState::DENIED:
        case IOState::RESTART:
        case IOState::STARTUP:
        case IOState::FAULT:
        case IOState::WELCOMED:
        case IOState::WELCOMING:
            ESP_LOGI(TAG, "Tried to go to a waiting state from a dissallowed state");
            return;
        default:
            break;
    }

    timer_refresh();

    switch (current_state) {
        case IOState::IDLE_WAITING:
            go_to_state(IOState::ALWAYS_ON_WAITING);
            break;
        case IOState::LOCKOUT_WAITING:
            go_to_state(IOState::IDLE_WAITING);
            break;
        case IOState::ALWAYS_ON_WAITING:
            go_to_state(IOState::LOCKOUT_WAITING);
            break;
        default:
            waiting_prior_state = current_state;
            go_to_state(IOState::LOCKOUT_WAITING);
            break;
    }
}

void handle_card_detected(IOEvent event) {
    IOState current_state;
    if (!IO::get_state(current_state)) {
        ESP_LOGE(TAG, "Failed to get state");
        return;
    }

    switch (current_state) {
        case IOState::IDLE:
            go_to_state(IOState::UNLOCKED);
            break;
        case IOState::LOCKOUT_WAITING:
            xTimerStop(waiting_timer, pdMS_TO_TICKS(100));
            go_to_state(IOState::LOCKOUT);
            break;
        case IOState::IDLE_WAITING:
            xTimerStop(waiting_timer, pdMS_TO_TICKS(100));
            go_to_state(IOState::IDLE);
            break;
        case IOState::ALWAYS_ON_WAITING:
            xTimerStop(waiting_timer, pdMS_TO_TICKS(100));
            go_to_state(IOState::ALWAYS_ON);
            break;
        case IOState::LOCKOUT:
            Buzzer::send_effect(SoundEffect::LOCKOUT_EFFECT);
            break;
        default:
            return;
    }
}

void handle_card_removed() {
    IOState current_state;
    if (!IO::get_state(current_state)) {
        ESP_LOGE(TAG, "Failed to get state");
        return;
    }

    switch (current_state) {
        case IOState::UNLOCKED:
            go_to_state(IOState::IDLE);
            break;
        default:
            return;
    }
}

void io_thread_fn(void *) {

    IOEvent current_event;

    while (true) {
        if (xQueueReceive(event_queue, &current_event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        switch (current_event.type) {
            case IOEventType::BUTTON_PRESSED:
                switch (current_event.button.type) {
                    case ButtonEventType::CLICK:
                        handle_button_clicked();
                        break;
                    case ButtonEventType::HELD:
                        go_to_state(IOState::RESTART);
                        break;
                    case ButtonEventType::RELEASED:
                        // TODO tell network to restart
                        esp_restart();
                        break;
                    default:
                        ESP_LOGI(TAG, "Unknown button event type recieved");
                        break;
                }
            break;

            case IOEventType::CARD_DETECTED:
                handle_card_detected(current_event);
            break;

            case IOEventType::CARD_REMOVED:
                handle_card_removed();
            break;

            case IOEventType::NETWORK_COMMAND:
                switch (current_event.network_command.type) {
                    case NetworkCommandEventType::COMMAND_STATE:
                        go_to_state(current_event.network_command.commanded_state);
                        break;
                    case NetworkCommandEventType::IDENTIFY:

                        break;
                    default:
                        ESP_LOGI(TAG, "Unkown network command type recieved");
                        break;
                }
            break;

            default:
                ESP_LOGI(TAG, "Unexpected event type recieved");
            break;
        }
    }
}

int IO::init() {
    event_queue = xQueueCreate(8, sizeof(IOEvent));
    animation_mutex = xSemaphoreCreateMutex();
    waiting_timer = xTimerCreate("waiting", pdMS_TO_TICKS(5000), pdFALSE, (void *) 0, waiting_timer_callback);

    if (event_queue == 0 || animation_mutex == NULL) {
        // TODO: Restart here
    }

    LED::init();
    Button::init();
    CardReader::init();
    Buzzer::init();

    xTaskCreate(io_thread_fn, "io", IO_TASK_STACK_SIZE, NULL, 0, &io_thread);
    return 0;
}

void IO::fault(FaultReason reason) {
    go_to_state(IOState::FAULT);
    // TODO: Log fault reason
};