#include "CardReader.hpp"

#include "drivers/mfrc630.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "common/pins.hpp"
#include "io/IO.hpp"

const char * TAG = "card";

#define CARD_TASK_STACK_SIZE 4000
TaskHandle_t card_thread;

const static spi_host_device_t spi_host = SPI2_HOST;
static spi_device_handle_t spi_device;

void mfrc630_SPI_transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) {
    spi_transaction_t transaction = {
        .length = (uint16_t) (len * 8),
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t ret = spi_device_transmit(spi_device, &transaction);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI Transmit Error: %d", ret);
    }
}

void mfrc630_SPI_select() {
    gpio_set_level(CS_NFC, 0);
}

void mfrc630_SPI_unselect() {
    gpio_set_level(CS_NFC, 1);
}

void card_reader_thread_fn(void *) {
    for (int i = 0; i < 0x47; i++) {
        ESP_LOGI(TAG, "Reg %x: %x", i, mfrc630_read_reg(i));
    }
    //while (true) {}
    while (true) {
        uint16_t atqa = mfrc630_iso14443a_REQA();

        if (atqa != 0) {
            uint8_t uid[10] = {0};
            uint8_t sak;

            mfrc630_iso14443a_select(uid, &sak);

            ESP_LOGI(TAG, "Read card UID: %02x%02x%02x%02x%02x%02x%02x%02x", uid[0], uid[1], uid[2], uid[3], uid[4], uid[5], uid[6], uid[7]);
            vTaskDelay(pdMS_TO_TICKS(5000));
        } else {
            ESP_LOGE(TAG, "Error reg: %d | Version reg: %d", mfrc630_read_reg(0x0A), mfrc630_read_reg(0x7F));
        }

        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

spi_bus_config_t spi_bus_config = {
    .mosi_io_num = SPI_MOSI,
    .miso_io_num = SPI_MISO,
    .sclk_io_num = SPI_CLK,
    .quadwp_io_num = -1,
    .quadhd_io_num = -1,
    .max_transfer_sz = 512 * 8,
};

spi_device_interface_config_t spi_device_config = {
    .dummy_bits = 0,
    .mode = 0,
    .clock_speed_hz = 1 * 1000 * 1000,
    .input_delay_ns = 50,
    .spics_io_num = CS_NFC,
    .flags = SPI_DEVICE_NO_DUMMY,
    .queue_size = 7,
};

void CardReader::init() {
    gpio_set_direction(SPI_MOSI, GPIO_MODE_OUTPUT);
    gpio_set_direction(SPI_MISO, GPIO_MODE_INPUT);
    gpio_set_direction(SPI_CLK, GPIO_MODE_OUTPUT);
    gpio_set_direction(CS_NFC, GPIO_MODE_OUTPUT);
    gpio_set_direction(NFC_PDOWN, GPIO_MODE_OUTPUT);

    gpio_set_level(NFC_PDOWN, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(NFC_PDOWN, 0);
    vTaskDelay(pdMS_TO_TICKS(5));
    
    if (spi_bus_initialize(spi_host, &spi_bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus");
        // TODO: Crash
    }

    esp_err_t add_device = spi_bus_add_device(spi_host, &spi_device_config, &spi_device);
    if (add_device != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI device");
        ESP_LOGE(TAG, "Error Code: %d", add_device);
        // TODO: Crash
    }

    mfrc630_AN1102_recommended_registers(MFRC630_PROTO_ISO14443A_106_MILLER_MANCHESTER);

    mfrc630_write_reg(0x28, 0x8E);
    mfrc630_write_reg(0x29, 0x15);
    mfrc630_write_reg(0x2A, 0x11);
    mfrc630_write_reg(0x2B, 0x06);
    
    xTaskCreate(card_reader_thread_fn, "card", CARD_TASK_STACK_SIZE, NULL, 0, &card_thread);
}