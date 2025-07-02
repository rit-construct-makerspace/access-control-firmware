#include "io/Speaker.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "driver/i2s_std.h"
#include "esp_log.h"
#include "common/pins.hpp"

static const char * TAG = "speaker";
#define SPEAKER_TASK_STACK_SIZE 2000
TaskHandle_t speaker_thread;

i2s_chan_handle_t tx_handle;
i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);

i2s_std_config_t std_cfg = {
    .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(48000),
    .slot_cfg = I2S_STD_MSB_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_STEREO),
    .gpio_cfg = {
        .mclk = I2SLRCLK,
        .bclk = I2SBCLK,
        .ws = I2S_GPIO_UNUSED,
        .dout = I2SDIN,
        .din = I2S_GPIO_UNUSED,
        .invert_flags = {
            .mclk_inv = false,
            .bclk_inv = false,
            .ws_inv = false,
        },
    },
};

int32_t source[10] = {1000000, 1000000, 1000000, 1000000, 1000000, -1000000, -1000000, -1000000, -1000000, -1000000};

void speaker_thread_fn(void *) {
    while (true) {
        esp_err_t ret = i2s_channel_write(tx_handle, source, 10, NULL, 1000);
        ESP_LOGI(TAG, "Ret val: %d", ret);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
}

int Speaker::init() {

    i2s_new_channel(&chan_cfg, &tx_handle, NULL);

    i2s_channel_init_std_mode(tx_handle, &std_cfg);

    i2s_channel_enable(tx_handle);

    //xTaskCreate(speaker_thread_fn, "speaker", SPEAKER_TASK_STACK_SIZE, NULL, 0, &speaker_thread);
    return 0;
}