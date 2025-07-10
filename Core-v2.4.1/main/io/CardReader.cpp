#include "CardReader.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include "common/pins.hpp"
#include "driver/gpio.h"
#include "driver/spi_common.h"
#include "driver/spi_master.h"
#include "drivers/mfrc630.h"
#include "esp_log.h"
#include "io/IO.hpp"

static const char* TAG = "card";

#define CARD_TASK_STACK_SIZE 4000
TaskHandle_t card_thread;
static SemaphoreHandle_t tag_mutex;

CardTagID card_tag = {};
bool card_detected = false;
uint8_t switch_error = 0;

const static spi_host_device_t spi_host = SPI3_HOST;
static spi_device_handle_t spi_device;

void mfrc630_SPI_transfer(const uint8_t* tx, uint8_t* rx, uint16_t len) {
    spi_transaction_t transaction = {
        .flags     = 0,
        .length    = (uint16_t)(len * 8),
        .rxlength  = 0,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    esp_err_t ret = spi_device_transmit(spi_device, &transaction);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI Transmit Error: %d", ret);
    }
}

void mfrc630_SPI_select() { gpio_set_level(CS_NFC, 0); }

void mfrc630_SPI_unselect() { gpio_set_level(CS_NFC, 1); }

void set_card_tag(CardTagID new_tag) {
    if (xSemaphoreTake(tag_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        card_tag = new_tag;
        xSemaphoreGive(tag_mutex);
    } else {
        // EXPLODE
    }
}

bool CardReader::get_card_tag(CardTagID& ret_tag) {
    if (xSemaphoreTake(tag_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ret_tag = card_tag;
        xSemaphoreGive(tag_mutex);
        return true;
    } else {
        return false;
    }
}

bool evaluate_switches() {
    bool sw1 = gpio_get_level(DET1);
    bool sw2 = gpio_get_level(DET2);

    if (sw1 && sw2) {
        switch_error = 0;
        return true;
    } else if (sw1 != sw2) {
        switch_error++;
        if (switch_error > 10) {
            IO::fault(FaultReason::CARD_SWITCH);
        }
        return false;
    } else {
        switch_error = 0;
        return false;
    }
}

void card_reader_thread_fn(void*) {
    uint8_t detect_allowed = 0;
    while (true) {
        bool card_present = evaluate_switches();

        while (card_present || card_detected) {
            uint16_t atqa = mfrc630_iso14443a_REQA();
            if (atqa != 0) { // Are there any cards that answered?
                uint8_t sak;
                uint8_t uid[10] = {0}; // uids are maximum of 10 bytes long.

                ESP_LOGD(TAG, "CARD DETECTED");

                // Select the card and discover its uid.
                uint8_t uid_len = mfrc630_iso14443a_select(uid, &sak);

                if (uid_len != 0) { // did we get an UID?

                    if (!card_detected && (detect_allowed <= 0)) {

                        CardTagID tag;
                        switch (uid_len) {
                        case 4:
                            tag = {
                                .type  = CardTagType::FOUR,
                                .value = {uid[0], uid[1], uid[2], uid[3]},
                            };
                            IO::send_event({
                                .type          = IOEventType::CARD_DETECTED,
                                .card_detected = {.card_tag_id = tag},
                            });
                            set_card_tag(tag);
                            break;
                        case 7:
                            tag = {
                                .type  = CardTagType::SEVEN,
                                .value = {uid[0], uid[1], uid[2], uid[3], uid[4],
                                        uid[5], uid[6]},
                            };
                            IO::send_event({
                                .type          = IOEventType::CARD_DETECTED,
                                .card_detected = {.card_tag_id = tag},
                            });
                            set_card_tag(tag);
                            break;
                        default:
                            IO::send_event({
                                .type = IOEventType::CARD_READ_ERROR,
                            });
                            break;
                        }

                        card_detected  = true;
                        detect_allowed = 6;
                    } else if (card_detected && (detect_allowed <= 0)) {
                        // Make sure no switcheroo was pulled
                        CardTagID det_tag;

                        switch (uid_len) {
                            case 4:
                                det_tag = {
                                    .type  = CardTagType::FOUR,
                                    .value = {uid[0], uid[1], uid[2], uid[3]},
                                };
                                break;
                            case 7:
                                det_tag = {
                                    .type  = CardTagType::SEVEN,
                                    .value = {uid[0], uid[1], uid[2], uid[3], uid[4],
                                            uid[5], uid[6]},
                                };
                                break;
                            default:
                                IO::send_event({
                                    .type = IOEventType::CARD_READ_ERROR,
                                });
                                break;
                        }

                        if (det_tag.type == CardTagType::FOUR ||
                            det_tag.type == CardTagType::SEVEN) {
                            CardTagID old_tag;
                            CardReader::get_card_tag(old_tag);

                            if (old_tag != det_tag) {

                                IO::send_event({
                                    .type = IOEventType::CARD_REMOVED,
                                    .card_removed = {
                                        .card_tag_id = old_tag,
                                    },
                                });

                                IO::send_event({
                                    .type = IOEventType::CARD_DETECTED,
                                    .card_detected = {
                                        .card_tag_id = det_tag,
                                    },
                                });
                                set_card_tag(det_tag);

                                ESP_LOGE(TAG, "SWITCHEROO DETECTED, SOUND THE ALARM :alert:");

                                detect_allowed = 6;
                            }
                        }
                    }

                    // Use the manufacturer default key...
                    uint8_t FFkey[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

                    mfrc630_cmd_load_key(FFkey); // load into the key buffer

                    // Try to athenticate block 0.
                    if (mfrc630_MF_auth(uid, MFRC630_MF_AUTH_KEY_A, 0)) {
                        ESP_LOGD(TAG, "Yay! We are authenticated!");

                        // Attempt to read the first 4 blocks.
                        uint8_t readbuf[16] = {0};
                        uint8_t len;
                        for (uint8_t b = 0; b < 4; b++) {
                            len = mfrc630_MF_read_block(b, readbuf);
                        }

                        mfrc630_MF_deauth(); // be sure to call this after an authentication!
                    } else {
                        ESP_LOGD(TAG, "Could not authenticate :(\n");
                    }
                } else {
                    ESP_LOGD(TAG, "Could not determine UID, perhaps some cards don't play");
                }
            } else {
                ESP_LOGD(TAG, "No answer to REQA, no cards?\n");
                if (card_detected) {
                    CardTagID old_tag;
                    CardReader::get_card_tag(old_tag);
                    IO::send_event({
                        .type = IOEventType::CARD_REMOVED,
                        .card_removed = {
                            .card_tag_id = old_tag,
                        },
                    });
                    card_detected = false;
                }
            }

            if (detect_allowed > 0) {
                detect_allowed--;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
        }

        vTaskDelay(pdMS_TO_TICKS(200));
    }
}

spi_bus_config_t spi_bus_config = {
    .mosi_io_num = SPI_MOSI,
    .miso_io_num = SPI_MISO,
    .sclk_io_num = SPI_CLK,

    .data2_io_num = GPIO_NUM_NC,
    .data3_io_num = GPIO_NUM_NC,
    .data4_io_num = GPIO_NUM_NC,
    .data5_io_num = GPIO_NUM_NC,
    .data6_io_num = GPIO_NUM_NC,
    .data7_io_num = GPIO_NUM_NC,

    .data_io_default_level = false,
    .max_transfer_sz       = 512 * 8,
    .flags                 = SPICOMMON_BUSFLAG_GPIO_PINS,
    .isr_cpu_id            = ESP_INTR_CPU_AFFINITY_AUTO,
};

spi_device_interface_config_t spi_device_config = {
    .dummy_bits     = 0,
    .mode           = 0,
    .clock_speed_hz = SPI_MASTER_FREQ_10M,
    .input_delay_ns = 0,
    .sample_point   = SPI_SAMPLING_POINT_PHASE_0,
    .spics_io_num   = -1,
    .flags          = SPI_DEVICE_NO_DUMMY,
    .queue_size     = 7,
    .pre_cb         = NULL,
    .post_cb        = NULL,
};

void CardReader::init() {

    gpio_config_t conf = {.pin_bit_mask =
                              (1ULL << CS_NFC) | (1ULL << NFC_PDOWN),
                          .mode         = (gpio_mode_t)(GPIO_MODE_OUTPUT),
                          .pull_up_en   = GPIO_PULLUP_DISABLE,
                          .pull_down_en = GPIO_PULLDOWN_DISABLE,
                          .intr_type    = GPIO_INTR_DISABLE};

    gpio_config(&conf);

    gpio_set_level(NFC_PDOWN, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(NFC_PDOWN, 0);
    vTaskDelay(pdMS_TO_TICKS(5));

    gpio_input_enable(DET1);
    gpio_input_enable(DET2);

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

    tag_mutex = xSemaphoreCreateMutex();

    // Set the registers of the MFRC630 into the default.
    mfrc630_AN1102_recommended_registers(MFRC630_PROTO_ISO14443A_106_MILLER_MANCHESTER);

    // Mystery registry changes
    mfrc630_write_reg(0x28, 0x8E);
    mfrc630_write_reg(0x29, 0xD5); // Set the transmit power to -1000 mV
    mfrc630_write_reg(0x2A, 0x11);
    mfrc630_write_reg(0x2B, 0x06);

    xTaskCreate(card_reader_thread_fn, "card", CARD_TASK_STACK_SIZE, NULL, 0,
                &card_thread);
}

bool CardReader::card_present() { return card_detected; }