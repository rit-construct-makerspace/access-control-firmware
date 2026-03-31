#include "hardware.hpp"
#include "driver/gpio.h"
#include "esp_efuse.h"
#include "pins.hpp"

namespace Hardware {
    // https://github.com/espressif/esp-idf/blob/5b11d5b26a8bf151fc6bac400158859eedd413bc/components/efuse/esp32s3/esp_efuse_table.csv#L218
    static constexpr size_t UNIQUE_ID_EFUSE_OFFSET = 0;
    static constexpr size_t UNIQUE_ID_EFUSE_SIZE_BYTES = 128 / 8;

    uint8_t serial_number_bytes[UNIQUE_ID_EFUSE_SIZE_BYTES] = {0};
    char serial_number_str[sizeof(serial_number_bytes) * 2 + 1] = {0};

    int init() {
        // Chip Serial Number
        esp_efuse_read_block(EFUSE_BLK_SYS_DATA_PART1, (void*)serial_number_bytes, UNIQUE_ID_EFUSE_OFFSET,
                             UNIQUE_ID_EFUSE_SIZE_BYTES * 8);
        for (size_t i = 0; i < sizeof(serial_number_bytes); i++) {
            snprintf(&serial_number_str[i * 2], 3, "%02X", serial_number_bytes[i]);
        }

        return 0;
    }

    const char* get_serial_number() {
        return serial_number_str;
    }

} // namespace Hardware
