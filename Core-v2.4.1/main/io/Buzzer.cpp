#include "Buzzer.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "common/pins.hpp"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"

TaskHandle_t buzzer_thread;
#define BUZZER_TASK_STACK_SIZE 2000
QueueHandle_t effect_queue;

static const char* TAG = "buzzer";

const ledc_channel_t ledc_channel = LEDC_CHANNEL_0;
const ledc_mode_t speed_mode = LEDC_LOW_SPEED_MODE;
const ledc_timer_t timer_num = LEDC_TIMER_0;
const ledc_timer_bit_t timer_bit = LEDC_TIMER_12_BIT;
const uint32_t idle_level = 0;

void start(uint32_t frequency) {
    uint32_t duty = (1 << timer_bit) / 2;

    esp_err_t err = ledc_set_duty(speed_mode, ledc_channel, duty);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty cycle");
        return;
    }
    err = ledc_update_duty(speed_mode, ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty cycle");
        return;
    }
    err = ledc_set_freq(speed_mode, timer_num, frequency);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set frequency");
        return;
    }
}

void stop() {
    BaseType_t err = ledc_set_duty(speed_mode, ledc_channel, 0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set duty cycle to zero");
    }
    err = ledc_update_duty(speed_mode, ledc_channel);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update duty cycle");
    }
    err = ledc_stop(speed_mode, ledc_channel, idle_level);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to stop buzzer");
    }
}

void play_effect(SoundEffect::Effect effect) {
    stop();
    for (int i = 0; i < effect.length; i++) {
        if (effect.notes[i].frequency == 0) {
            stop();
        } else {
            start(effect.notes[i].frequency);
        }
        vTaskDelay(pdMS_TO_TICKS(effect.notes[i].duration));
        stop();
    }
    stop();
}

void buzzer_task_fn(void*) {
    stop();

    SoundEffect::Effect current_effect;

    while (true) {
        if (xQueueReceive(effect_queue, &current_effect, portMAX_DELAY) != pdTRUE) {
            continue;
        }
        play_effect(current_effect);
    }
}

void setup() {

    const ledc_timer_config_t timer_config = {
        .speed_mode = speed_mode,
        .duty_resolution = LEDC_TIMER_12_BIT,
        .timer_num = timer_num,
        .freq_hz = 4000,
    };

    ledc_timer_config(&timer_config);

    const ledc_channel_config_t channel_config = {
        .gpio_num = BUZZER_PIN,
        .speed_mode = speed_mode,
        .channel = ledc_channel,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = timer_num,
        .duty = 0,
        .hpoint = 0,

    };

    ledc_channel_config(&channel_config);
}

int Buzzer::init() {

    effect_queue = xQueueCreate(1, sizeof(SoundEffect::Effect));

    gpio_config_t conf = {.pin_bit_mask = 1ULL << BUZZER_PIN,
                          .mode = GPIO_MODE_OUTPUT,
                          .pull_up_en = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type = GPIO_INTR_DISABLE};

    gpio_config(&conf);

    setup();

    xTaskCreate(buzzer_task_fn, "buzzer", BUZZER_TASK_STACK_SIZE, NULL, 0, &buzzer_thread);
    return 0;
}

bool Buzzer::send_effect(SoundEffect::Effect effect) {
    return xQueueSend(effect_queue, &effect, pdTICKS_TO_MS(100));
}