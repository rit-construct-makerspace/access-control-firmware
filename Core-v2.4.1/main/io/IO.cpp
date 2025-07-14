#include "IO.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>
#include <freertos/timers.h>

#include "common/pins.hpp"
#include "driver/gpio.h"
#include "esp_log.h"
#include "io/Button.hpp"
#include "io/Buzzer.hpp"
#include "io/CardReader.hpp"
#include "io/LEDControl.hpp"
#include "io/Temperature.hpp"
#include "network/network.hpp"

static const char* TAG = "io";

QueueHandle_t event_queue;
TaskHandle_t io_thread;

#define IO_TASK_STACK_SIZE 4000

static IOState state = IOState::STARTUP;
static IOState prior_request_state;
static SemaphoreHandle_t animation_mutex;

bool IO::get_state(IOState& send_state) {
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
        LED::set_animation(&Animation::RESTART);
        return;
    }

    if (current_state == IOState::FAULT) {
        ESP_LOGE(TAG, "Attempted to go to %s from FAULT", io_state_to_string(next_state));
        return;
    }

    switch (next_state) {
        case IOState::IDLE:
            gpio_set_level(SWITCH_CNTRL, 0);
            CardReader::set_require_switches(true);
            LED::set_animation(&Animation::IDLE);
            break;
        case IOState::UNLOCKED:
            gpio_set_level(SWITCH_CNTRL, 1);
            CardReader::set_require_switches(true);
            Buzzer::send_effect(SoundEffect::ACCEPTED);
            LED::set_animation(&Animation::UNLOCKED);
            break;
        case IOState::ALWAYS_ON:
            gpio_set_level(SWITCH_CNTRL, 1);
            CardReader::set_require_switches(true);
            Buzzer::send_effect(SoundEffect::ACCEPTED);
            LED::set_animation(&Animation::ALWAYS_ON);
            break;
        case IOState::LOCKOUT:
            gpio_set_level(SWITCH_CNTRL, 0);
            CardReader::set_require_switches(true);
            Buzzer::send_effect(SoundEffect::LOCKOUT);
            LED::set_animation(&Animation::LOCKOUT);
            break;
        case IOState::NEXT_CARD:
            gpio_set_level(SWITCH_CNTRL, 0);
            CardReader::set_require_switches(true);
            LED::set_animation(&Animation::NEXT_CARD);
            break;
        case IOState::WELCOMING:
            gpio_set_level(SWITCH_CNTRL, 0);
            CardReader::set_require_switches(false);
            LED::set_animation(&Animation::WELCOMING);
            break;
        case IOState::WELCOMED:
            gpio_set_level(SWITCH_CNTRL, 0);
            CardReader::set_require_switches(false);
            Buzzer::send_effect(SoundEffect::ACCEPTED);
            LED::set_animation(&Animation::WELCOMED);
            break;
        case IOState::ALWAYS_ON_WAITING:
            CardReader::set_require_switches(true);
            LED::set_animation(&Animation::ALWAYS_ON_WAITING);
            break;
        case IOState::LOCKOUT_WAITING:
            CardReader::set_require_switches(true);
            LED::set_animation(&Animation::LOCKOUT_WAITING);
            break;
        case IOState::IDLE_WAITING:
            CardReader::set_require_switches(true);
            LED::set_animation(&Animation::IDLE_WAITING);
            break;
        case IOState::AWAIT_AUTH:
            LED::set_animation(&Animation::AWAIT_AUTH);
            break;
        case IOState::DENIED:
            Buzzer::send_effect(SoundEffect::DENIED);
            LED::set_animation(&Animation::DENIED);
            break;
        case IOState::FAULT:
            gpio_set_level(SWITCH_CNTRL, 0);
            Buzzer::send_effect(SoundEffect::FAULT);
            LED::set_animation(&Animation::FAULT);
            break;
        default:
            ESP_LOGI(TAG, "Attempted to go to an unkown state");
            return;
    }

    if (!set_state(next_state)) {
        ESP_LOGI(TAG, "Failed to update the stored state");
        IO::fault(FaultReason::MUTEX_ERROR);
    }
}

TimerHandle_t waiting_timer;

void waiting_timer_callback(TimerHandle_t timer) {
    go_to_state(prior_request_state);
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
            ESP_LOGI(TAG, "Tried to go to a waiting state from %s", io_state_to_string(current_state));
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
            prior_request_state = current_state;
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
            prior_request_state = IOState::IDLE;
            go_to_state(IOState::AWAIT_AUTH);
            Network::send_event({
                .type = NetworkEventType::AuthRequest,
                .auth_request =
                    {
                        .requester = event.card_detected.card_tag_id,
                        .to_state = IOState::UNLOCKED,
                    },
            });
            break;
        case IOState::LOCKOUT_WAITING:
            xTimerStop(waiting_timer, pdMS_TO_TICKS(100));
            go_to_state(IOState::AWAIT_AUTH);
            Network::send_event({
                .type = NetworkEventType::AuthRequest,
                .auth_request =
                    {
                        .requester = event.card_detected.card_tag_id,
                        .to_state = IOState::LOCKOUT,
                    },
            });
            break;
        case IOState::IDLE_WAITING:
            xTimerStop(waiting_timer, pdMS_TO_TICKS(100));
            go_to_state(IOState::AWAIT_AUTH);
            Network::send_event({.type = NetworkEventType::AuthRequest,
                                 .auth_request = {
                                     .requester = event.card_detected.card_tag_id,
                                     .to_state = IOState::IDLE,
                                 }});
            break;
        case IOState::ALWAYS_ON_WAITING:
            xTimerStop(waiting_timer, pdMS_TO_TICKS(100));
            go_to_state(IOState::AWAIT_AUTH);
            Network::send_event({
                .type = NetworkEventType::AuthRequest,
                .auth_request =
                    {
                        .requester = event.card_detected.card_tag_id,
                        .to_state = IOState::ALWAYS_ON,
                    },
            });
            break;
        case IOState::LOCKOUT:
            Buzzer::send_effect(SoundEffect::LOCKOUT);
            break;
        case IOState::WELCOMING:
            go_to_state(IOState::AWAIT_AUTH);
            Network::send_event({
                .type = NetworkEventType::AuthRequest,
                .auth_request =
                    {
                        .requester = event.card_detected.card_tag_id,
                        .to_state = IOState::WELCOMED,
                    },
            });
            break;
        case IOState::NEXT_CARD:
            Network::send_event({
                .type = NetworkEventType::StateChange,
                .state_change =
                    {
                        .from = IOState::NEXT_CARD,
                        .to = IOState::UNLOCKED,
                        .reason = StateChangeReason::CardActivated,
                        .who = event.card_detected.card_tag_id,
                    },
            });
            go_to_state(IOState::UNLOCKED);
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
            Network::send_event({
                .type = NetworkEventType::StateChange,
                .state_change =
                    {
                        .from = current_state,
                        .to = IOState::IDLE,
                        .reason = StateChangeReason::CardRemoved,
                        .who = {},
                    },
            });
            go_to_state(IOState::IDLE);
            break;
        case IOState::AWAIT_AUTH:
            go_to_state(prior_request_state);
            break;
        case IOState::WELCOMED:
            go_to_state(IOState::WELCOMING);
            break;
        default:
            return;
    }
}

TimerHandle_t identify_timer;

void identify_timer_callback(TimerHandle_t timer) {
    IOState current_state;
    IO::get_state(current_state);

    switch (current_state) {
        case IOState::IDLE:
            LED::set_animation(&Animation::IDLE);
            break;
        case IOState::UNLOCKED:
            LED::set_animation(&Animation::UNLOCKED);
            break;
        case IOState::ALWAYS_ON:
            LED::set_animation(&Animation::ALWAYS_ON);
            break;
        case IOState::LOCKOUT:
            LED::set_animation(&Animation::LOCKOUT);
            break;
        case IOState::NEXT_CARD:
            LED::set_animation(&Animation::NEXT_CARD);
            break;
        case IOState::WELCOMING:
            LED::set_animation(&Animation::WELCOMING);
            break;
        case IOState::WELCOMED:
            LED::set_animation(&Animation::WELCOMED);
            break;
        case IOState::ALWAYS_ON_WAITING:
            LED::set_animation(&Animation::ALWAYS_ON_WAITING);
            break;
        case IOState::LOCKOUT_WAITING:
            LED::set_animation(&Animation::LOCKOUT_WAITING);
            break;
        case IOState::IDLE_WAITING:
            LED::set_animation(&Animation::IDLE_WAITING);
            break;
        case IOState::AWAIT_AUTH:
            LED::set_animation(&Animation::AWAIT_AUTH);
            break;
        case IOState::DENIED:
            LED::set_animation(&Animation::DENIED);
            break;
        case IOState::FAULT:
            LED::set_animation(&Animation::FAULT);
            break;
        default:
            ESP_LOGI(TAG, "Failed to set LEDs after identify");
            return;
    }
}

void handle_identify() {
    LED::set_animation(&Animation::IDENTIFY);
    Buzzer::send_effect(SoundEffect::MARIO_VICTORY);
    xTimerStart(identify_timer, pdMS_TO_TICKS(100));
}

TimerHandle_t denied_timer;

void denied_timer_callback(TimerHandle_t timer) {
    go_to_state(prior_request_state);
}

void handle_denied() {
    go_to_state(IOState::DENIED);
    xTimerStart(denied_timer, pdMS_TO_TICKS(100));
}

void handle_network_command(IOEvent current_event) {
    switch (current_event.network_command.type) {
        case NetworkCommandEventType::COMMAND_STATE:

            IOState cur_state;
            IO::get_state(cur_state);

            if (current_event.network_command.requested) {

                if (cur_state != IOState::AWAIT_AUTH) {
                    return;
                }

                switch (current_event.network_command.commanded_state) {
                    case IOState::ALWAYS_ON:
                    case IOState::LOCKOUT:
                    case IOState::IDLE:
                        Network::send_event({
                            .type = NetworkEventType::StateChange,
                            .state_change =
                                {
                                    .from = cur_state,
                                    .to = current_event.network_command.commanded_state,
                                    .reason = StateChangeReason::ButtonPress,
                                    .who = {},
                                },
                        });
                        break;
                    case IOState::UNLOCKED:
                        Network::send_event({
                            .type = NetworkEventType::StateChange,
                            .state_change =
                                {
                                    .from = cur_state,
                                    .to = current_event.network_command.commanded_state,
                                    .reason = StateChangeReason::CardActivated,
                                    .who = CardTagID{},
                                },
                        });
                        break;
                    default:
                        // FREAK OUT
                        return;
                }
            } else {
                Network::send_event({
                    .type = NetworkEventType::StateChange,
                    .state_change =
                        {
                            .from = cur_state,
                            .to = current_event.network_command.commanded_state,
                            .reason = StateChangeReason::ServerCommanded,
                            .who = {},
                        },
                });
            }
            go_to_state(current_event.network_command.commanded_state);
            break;
        case NetworkCommandEventType::IDENTIFY:
            handle_identify();
            break;
        case NetworkCommandEventType::DENY:
            handle_denied();
            break;
        default:
            ESP_LOGI(TAG, "Unkown network command type recieved");
            break;
    }
}

bool allowed_fault_event(IOEvent event) {
    return (event.type == IOEventType::BUTTON_PRESSED ||
            (event.type == IOEventType::NETWORK_COMMAND && event.network_command.commanded_state == IOState::RESTART));
}

void io_thread_fn(void*) {

    IOEvent current_event = {};

    while (true) {
        if (xQueueReceive(event_queue, &current_event, portMAX_DELAY) != pdTRUE) {
            continue;
        }

        IOState current_state;
        if (IO::get_state(current_state) && current_state == IOState::FAULT) {
            if (!allowed_fault_event(current_event)) {
                continue;
            }
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
                        Network::send_event({
                            .type = NetworkEventType::PleaseRestart,
                        });
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

            case IOEventType::CARD_READ_ERROR:
                Buzzer::send_effect(SoundEffect::DENIED);
                break;

            case IOEventType::NETWORK_COMMAND:
                handle_network_command(current_event);
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
    waiting_timer = xTimerCreate("waiting", pdMS_TO_TICKS(5000), pdFALSE, (void*)0, waiting_timer_callback);
    identify_timer = xTimerCreate("identify", pdMS_TO_TICKS(9630), pdFALSE, (void*)0, identify_timer_callback);
    denied_timer = xTimerCreate("denied", pdMS_TO_TICKS(1500), pdFALSE, (void*)0, denied_timer_callback);

    if (event_queue == 0 || animation_mutex == NULL) {
        // TODO: Restart here
    }

    gpio_config_t conf = {
        .pin_bit_mask = (1ULL << SWITCH_CNTRL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    gpio_config(&conf); // Enable turning on and off the connected switch

    LED::init();
    Button::init();
    CardReader::init();
    Buzzer::init();
    Temperature::init();

    xTaskCreate(io_thread_fn, "io", CONFIG_IO_TASK_STACK_SIZE, NULL, 0, &io_thread);
    return 0;
}

void IO::fault(FaultReason reason) {
    IOState cur_state;
    IO::get_state(cur_state);

    if (reason == FaultReason::START_FAIL) {
        return; // TODO: figure out what to do
    }

    go_to_state(IOState::FAULT);

    Network::send_event({
        .type = NetworkEventType::StateChange,
        .state_change =
            {
                .from = cur_state,
                .to = IOState::FAULT,
                .reason = fault_reason_to_state_change_reason(reason),
                .who = {},
            },
    });
};
