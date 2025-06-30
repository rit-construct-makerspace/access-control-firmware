/*
 * SPDX-FileCopyrightText: 2022-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "io/LEDControl.hpp"
#include "network/network.hpp"
#include "network/usb.hpp"
#include <stdint.h>

static const char* TAG = "example";

const char* string_descriptor = "im a shlug";
extern "C" void app_main(void) {
    USB::init();
    led_init();
    set_led_state(LEDDisplayState::IDLE);
    Network::init();
}