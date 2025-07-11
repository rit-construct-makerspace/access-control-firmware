#include "ota.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#

namespace OTA {
    static const char* TAG = "ota";
    static const esp_partition_t* active_ota_part = NULL;

    void begin(OTATag tag) {
        const esp_partition_t* running_part = esp_ota_get_running_partition();
        esp_app_desc_t running_description;
        esp_err_t err = esp_ota_get_partition_description(running_part, &running_description);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get information about currently running app");
        } else {
            ESP_LOGI(TAG, "========   CURRENT PROJECT   ========");
            ESP_LOGI(TAG, "= Project: %.32s", running_description.project_name);
            ESP_LOGI(TAG, "= Version: %.32s", running_description.version);
            ESP_LOGI(TAG, "= %.16s %.16s", running_description.date, running_description.time);
            ESP_LOGI(TAG, "= ESP IDF Version %.32s", running_description.idf_ver);
            ESP_LOGI(TAG, "=====================================");
        }
        active_ota_part = esp_ota_get_next_update_partition(NULL);
        if (active_ota_part == NULL) {
            ESP_LOGE(TAG, "OTA Failed, no next partition available");
        }
        ESP_LOGI(TAG, "Updating to %.32s", tag.data());
    }

    void feed_bytes(uint8_t* buf, std::size_t buflen) {
    }

    void finish() {
    }

} // namespace OTA