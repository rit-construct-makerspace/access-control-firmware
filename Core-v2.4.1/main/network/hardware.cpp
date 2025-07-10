#include "common/hardware.hpp"
#include "common/pins.hpp"
#include "driver/gpio.h"
#include "esp_efuse.h"

namespace Hardware {
    // https://github.com/espressif/esp-idf/blob/5b11d5b26a8bf151fc6bac400158859eedd413bc/components/efuse/esp32s3/esp_efuse_table.csv#L218
    static constexpr size_t UNIQUE_ID_EFUSE_OFFSET = 0;
    static constexpr size_t UNIQUE_ID_EFUSE_SIZE_BYTES = 128 / 8;

    uint8_t serial_number_bytes[UNIQUE_ID_EFUSE_SIZE_BYTES] = {0};
    char serial_number_str[sizeof(serial_number_bytes) * 2 + 1] = {0};

    HardwareEdition edition;
    const char* edition_string = NULL;

    int init() {
        // Chip Serial Number
        esp_efuse_read_block(EFUSE_BLK_SYS_DATA_PART1, (void*)serial_number_bytes, UNIQUE_ID_EFUSE_OFFSET,
                             UNIQUE_ID_EFUSE_SIZE_BYTES * 8);
        for (size_t i = 0; i < sizeof(serial_number_bytes); i++) {
            snprintf(&serial_number_str[i * 2], 3, "%02X", serial_number_bytes[i]);
        }

        // Hardware mode
        gpio_input_enable(MODE);
        edition = (HardwareEdition)gpio_get_level(MODE);
        if (edition == HardwareEdition::LITE) {
            edition_string = "ACS 2.4.1 Core Lite";
        } else if (edition == HardwareEdition::STANDARD) {
            edition_string = "ACS 2.4.1 Core";
        } else {
            edition_string = "Unknown Hardware Type";
        }

        return 0;
    }

    const char* get_serial_number() {
        return serial_number_str;
    }

    HardwareEdition get_edition() {
        return edition;
    }
    const char* get_edition_string() {
        return edition_string;
    }

} // namespace Hardware
