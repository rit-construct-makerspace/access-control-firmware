#include "io/Temperature.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "common/pins.hpp"
#include "ds18b20.h"
#include "esp_log.h"
#include "io/IO.hpp"
#include "network/storage.hpp"
#include "onewire_bus.h"
#include "onewire_device.h"

#define MAX_ONEWIRE_DEVICES 64
onewire_bus_handle_t onewire_bus;
uint8_t num_ds_detcted = 0;
ds18b20_device_handle_t s_ds18b20s[MAX_ONEWIRE_DEVICES];
static float s_temperature[MAX_ONEWIRE_DEVICES];

SemaphoreHandle_t temp_mutex;
static float cur_temp = 0.0;

#define TEMP_TASK_STACK_SIZE 2000
TaskHandle_t temp_thread;

static const char* TAG = "temp";

void sensor_detect() {
    int onewire_device_found = 0;
    onewire_device_iter_handle_t iter = NULL;
    onewire_device_t next_onewire_device;
    esp_err_t search_result = ESP_OK;

    onewire_new_device_iter(onewire_bus, &iter);

    while (search_result == ESP_OK) {
        search_result = onewire_device_iter_get_next(iter, &next_onewire_device);

        if (search_result == ESP_OK) {
            ds18b20_config_t ds_cfg = {};

            if (ds18b20_new_device(&next_onewire_device, &ds_cfg, &s_ds18b20s[num_ds_detcted]) == ESP_OK) {
                num_ds_detcted++;
            } else {
                ESP_LOGI(TAG, "Found an unknown device, address: %016llX", next_onewire_device.address);
            }

            onewire_device_found++;
            if (onewire_device_found >= MAX_ONEWIRE_DEVICES) {
                break;
            }
        }
    }
}

void sensor_read() {
    float temp_temp = 0.0;
    for (int i = 0; i < num_ds_detcted; i++) {
        ds18b20_trigger_temperature_conversion(s_ds18b20s[i]);
        ds18b20_get_temperature(s_ds18b20s[i], &temp_temp);
        s_temperature[i] = temp_temp;
    }
}
extern bool ok_to_rmt_read;
void temp_thread_fn(void*) {
    while (true) {
        if (!ok_to_rmt_read) {
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        sensor_read();

        float max = 1.0;
        float min = 100.0f;

        for (int i = 0; i < num_ds_detcted; i++) {
            if (s_temperature[i] > max) {
                max = s_temperature[i];
            }

            if (s_temperature[i] < min) {
                min = s_temperature[i];
            }
        }

        if (xSemaphoreTake(temp_mutex, pdMS_TO_TICKS(50)) == pdTRUE) {
            cur_temp = max;
            xSemaphoreGive(temp_mutex);
        } // If we fail to get the mutex just drop the new data, not a big deal

        if (max >= Storage::get_max_temp() || min <= 0) {
            ESP_LOGE(TAG, "MAX: %f | MIN: %f", max, min);
            IO::fault(FaultReason::TEMP_ERROR);
        }

        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int Temperature::init() {

    onewire_bus_config_t bus_config = {
        .bus_gpio_num = TEMP_PIN,
        .flags = {.en_pull_up = 1},
    };

    onewire_bus_rmt_config_t rmt_config = {
        .max_rx_bytes = 10,
    };

    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    onewire_new_bus_rmt(&bus_config, &rmt_config, &onewire_bus);

    temp_mutex = xSemaphoreCreateMutex();

    sensor_detect();

    for (int i = 0; i < MAX_ONEWIRE_DEVICES; i++) {
        s_temperature[i] = 1.0f;
    }

    xTaskCreate(temp_thread_fn, "temp", CONFIG_TEMP_TASK_STACK_SIZE, NULL, 0, &temp_thread);
    return 0;
}

bool Temperature::get_temp(float& ret_temp) {
    if (xSemaphoreTake(temp_mutex, 100) == pdTRUE) {
        ret_temp = cur_temp;
        xSemaphoreGive(temp_mutex);
        return true;
    } else {
        return false;
    }
}