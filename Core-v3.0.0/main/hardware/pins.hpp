#pragma once
#include "hal/gpio_types.h"

constexpr gpio_num_t BUTTON_PIN = GPIO_NUM_0;
constexpr gpio_num_t BUZZER_PIN = GPIO_NUM_1;
constexpr gpio_num_t NFC_PDOWN = GPIO_NUM_4;
#define LED_PIN 7
constexpr gpio_num_t SPI_MISO = GPIO_NUM_11;
constexpr gpio_num_t SPI_MOSI = GPIO_NUM_12;
constexpr gpio_num_t SPI_CLK = GPIO_NUM_13;
constexpr gpio_num_t CARD_DET2 = GPIO_NUM_26;
constexpr gpio_num_t CS_NFC = GPIO_NUM_33;
#define DEBUG_LED_PIN 41
constexpr gpio_num_t SWITCH_CNTRL = GPIO_NUM_46;
constexpr gpio_num_t CARD_DET1 = GPIO_NUM_47;
