#include "ota.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "network.hpp"
#include "string.h"

#include "http_manager.hpp"

namespace OTA {
    static const char* TAG = "ota";
    static const esp_partition_t* active_ota_part = NULL;
    static esp_ota_handle_t ota_handle = 0;

    void begin(OTATag tag) {
        const esp_partition_t* running_part = esp_ota_get_running_partition();
        esp_app_desc_t running_description;
        esp_err_t err = esp_ota_get_partition_description(running_part, &running_description);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get information about currently running app");
        } else {
            ESP_LOGI(TAG, "========   CURRENT PROJECT   ========");
            ESP_LOGI(TAG, "Project: %.32s", running_description.project_name);
            ESP_LOGI(TAG, "Version: %.32s", running_description.version);
            ESP_LOGI(TAG, "%.16s %.16s", running_description.date, running_description.time);
            ESP_LOGI(TAG, "ESP IDF Version %.32s", running_description.idf_ver);
        }
        active_ota_part = esp_ota_get_next_update_partition(NULL);
        if (active_ota_part == NULL) {
            ESP_LOGE(TAG, "OTA Failed, no next partition available");
            return;
        }
        ESP_LOGI(TAG, "Updating to %.32s", tag.data());

        err = esp_ota_begin(active_ota_part, OTA_SIZE_UNKNOWN, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start OTA: %s", esp_err_to_name(err));
            return;
        }
#ifdef DEV_SERVER
        std::string ota_url = "http://";
        ota_url += DEV_SERVER;
        ota_url += ":3000/api/files/ota/" + std::string(tag.data(), tag.size()) + "/Core.bin";
#else
        std::string ota_url = "https://";
        ota_url += Storage::get_server();
        ota_url += "/api/files/ota/" + std::string(tag.data(), tag.size()) + "/Core.bin";
#endif

        std::string* ptr_url = new std::string(ota_url);

        HTTPManager::Transfer xfer = {
            .type = HTTPManager::OperationType::GET,
            .start =
                [](void*vp_url, const char** url) {
                    *url = ((std::string*)vp_url)->data();
                    return ESP_OK;
                },
            .data =
                [](void*, uint8_t* data, size_t* len) {
                    esp_err_t err = esp_ota_write(ota_handle, data, *len);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to write OTA data, cancelling: %s", esp_err_to_name(err));
                        return err;
                    }
                    return ESP_OK;
                },
            .finish =
                [](void*vp, esp_err_t err) {
                    if (err != ESP_OK) {
                        std::string smsg = std::string("Failed to download OTA update: ")+esp_err_to_name(err);
                        ESP_LOGE(TAG, "%s", smsg.c_str());
                        char *msg = new char[smsg.size()];
                        strcpy(msg, smsg.data());
                        Network::send_event(NetworkEvent{
                            .type = NetworkEventType::Message,
                            .message = msg,
                        });
                        return;
                    }
                    err = esp_ota_end(ota_handle);
                    if (err != ESP_OK) {
                        std::string smsg = std::string("Failed to install OTA update: ")+esp_err_to_name(err);
                        ESP_LOGE(TAG, "%s", smsg.c_str());
                        char *msg = new char[smsg.size()];
                        strcpy(msg, smsg.data());
                        Network::send_event(NetworkEvent{
                            .type = NetworkEventType::Message,
                            .message = msg,
                        });
                        return;
                    }
                    esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
                    delete (std::string*)(vp);
                },
            .user_data = (void*)ptr_url,
        };
        HTTPManager::queue_transfer(xfer);
    } // namespace OTA

    // void feed_bytes(uint8_t* buf, std::size_t buflen) {
    // }

    void finish() {
    }

} // namespace OTA