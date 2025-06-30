#include "CardReader.hpp"

#include "drivers/mfrc630.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "esp_log.h"
#include "common/pins.hpp"

const char * TAG = "card";

const static spi_host_device_t spi_host = SPI1_HOST;
static spi_device_handle_t spi_device;

void mfrc630_SPI_transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) {

}

void mfrc630_SPI_select() {

}

void mfrc630_SPI_unselect() {
    
}

void card_reader_thread_fn(void *) {

}

spi_bus_config_t spi_bus_config = {
    .mosi_io_num = SPI_MOSI,
    .miso_io_num = SPI_MISO,
    .sclk_io_num = SPI_CLK,
};

spi_device_interface_config_t spi_device_config = {
    .spics_io_num = CS_NFC,
};

void CardReader::init() {
    if (spi_bus_initialize(spi_host, &spi_bus_config, SPI_DMA_CH_AUTO) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to initialize SPI bus");
        // TODO: Crash
    }

    if (spi_bus_add_device(spi_host, &spi_device_config, &spi_device) != ESP_OK) {
        ESP_LOGI(TAG, "Failed to initialize SPI device");
        // TODO: Crash
    }
    
}