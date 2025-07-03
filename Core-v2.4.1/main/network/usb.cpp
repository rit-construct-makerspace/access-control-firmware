#include "usb.hpp"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "sdkconfig.h"
#include "soc/rtc_cntl_reg.h"
#include "tinyusb.h"
#include "tusb_cdc_acm.h"

#define LOG_CDC_ITF ((tinyusb_cdcacm_itf_t)0)
#define MAX_DEBUG_SIZE 3072
#define DEBUG_STREAM_BUF_SIZE (2 * MAX_DEBUG_SIZE)

static const char* TAG = "usb";

static SemaphoreHandle_t stream_buffer_lock;
static StreamBufferHandle_t debug_stream_buffer;

int write_to_usb_task(const unsigned char* buf, size_t buf_len) {
    if (!xSemaphoreTake(stream_buffer_lock, pdMS_TO_TICKS(100))) {
        return -1;
    }

    size_t written = xStreamBufferSend(debug_stream_buffer, (void*)buf, buf_len,
                                       pdMS_TO_TICKS(100));
    xSemaphoreGive(stream_buffer_lock);
    return written;
}

/**
 * @brief CDC device RX callback
 *
 * CDC device signals, that new data were received
 *
 * @param[in] itf   CDC device index
 * @param[in] event CDC event type
 */
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t* event) {
    static uint8_t rx_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE + 1];
    /* initialization */
    size_t rx_size = 0;

    /* read */
    esp_err_t ret =
        tinyusb_cdcacm_read(static_cast<tinyusb_cdcacm_itf_t>(itf), rx_buf,
                            CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rx_size);
    if (ret == ESP_OK) {
        write_to_usb_task(rx_buf, rx_size);
    } else {
        // Had an error (don't log tho or infinite loop of logging)
    }
}

void restart_to_firmware_download() {
    REG_WRITE(RTC_CNTL_OPTION1_REG, RTC_CNTL_FORCE_DOWNLOAD_BOOT);
    esp_restart();
}

/**
 * @brief CDC device line change callback
 *
 * CDC device signals, that the DTR, RTS states changed
 *
 * @param[in] itf   CDC device index
 * @param[in] event CDC event type
 */
int dtr = -1;
int rts = -1;

bool rts_ever = false;

void tinyusb_cdc_line_state_changed_callback(int itf, cdcacm_event_t* event) {
    int new_dtr = event->line_state_changed_data.dtr;
    int new_rts = event->line_state_changed_data.rts;
    if (new_rts) {
        rts_ever = true;
    }
    
    dtr = new_dtr;
    rts = new_rts;
}

static unsigned char* mike_buf = nullptr;

extern "C" int usb_log_vprintf(const char* fmt, va_list args) {

    size_t mike_buflen = vsnprintf((char*)mike_buf, MAX_DEBUG_SIZE, fmt, args);
    mike_buf[mike_buflen]     = '\r';
    mike_buf[mike_buflen + 1] = 0;

    return write_to_usb_task(mike_buf, mike_buflen + 1);
}

#define USB_TASK_STACK_SIZE 4000
TaskHandle_t usb_thread;

void usb_thread_fn(void*) {
    static unsigned char usb_buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE] = {0};
    while (!rts_ever) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    ESP_LOGI(TAG, "dtr: %d, rts: %d", dtr, rts);
    while (1) {
        size_t read = xStreamBufferReceive(debug_stream_buffer, (void*)usb_buf,
                                           CONFIG_TINYUSB_CDC_RX_BUFSIZE,
                                           pdMS_TO_TICKS(100));
        if (read > 0) {
            tinyusb_cdcacm_write_queue(LOG_CDC_ITF, usb_buf, read);
            esp_err_t err = tinyusb_cdcacm_write_flush(LOG_CDC_ITF, pdMS_TO_TICKS(100));
            if (err != ESP_OK){
                xStreamBufferReset(debug_stream_buffer);
            }
        }
    }
}

esp_err_t USB::init() {
    stream_buffer_lock  = xSemaphoreCreateMutex();
    debug_stream_buffer = xStreamBufferCreate(DEBUG_STREAM_BUF_SIZE, 32);
    mike_buf = (unsigned char*)malloc(MAX_DEBUG_SIZE);

    assert(stream_buffer_lock);
    assert(debug_stream_buffer);
    esp_log_set_vprintf(usb_log_vprintf);

    ESP_LOGI(TAG, "USB initialization begin");

    const tinyusb_config_t tusb_cfg = {
        .device_descriptor        = NULL,
        .string_descriptor        = NULL,
        .external_phy             = false,
        .configuration_descriptor = NULL,
        .self_powered             = true,
        .vbus_monitor_io          = -1,
    };

    ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

    tinyusb_config_cdcacm_t acm_cfg = {
        .usb_dev          = TINYUSB_USBDEV_0,
        .cdc_port         = TINYUSB_CDC_ACM_0,
        .rx_unread_buf_sz = 64,
        .callback_rx      = &tinyusb_cdc_rx_callback, // the first way to
                                                      // register a callback
        .callback_rx_wanted_char      = NULL,
        .callback_line_state_changed  = tinyusb_cdc_line_state_changed_callback,
        .callback_line_coding_changed = NULL};

    ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));

    // custom log handler
    ESP_LOGI(TAG, "USB initialization DONE");

    xTaskCreate(usb_thread_fn, "usb", USB_TASK_STACK_SIZE, NULL, 0,
                &usb_thread);

    return 0;
}
