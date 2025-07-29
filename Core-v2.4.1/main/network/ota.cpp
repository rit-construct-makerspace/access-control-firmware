#include "ota.hpp"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "network.hpp"
#include "storage.hpp"
#include "string.h"

#include "http_manager.hpp"

extern bool ok_to_rmt_read;
namespace OTA {
    static const char* TAG = "ota";
    static const esp_partition_t* active_ota_part = NULL;
    static esp_ota_handle_t ota_handle = 0;

    std::string active_version = "";
    std::string next_version = "";

    std::string running_app_version() {
        return active_version;
    }
    std::string next_app_version() {
        return next_version;
    }

    void begin(OTATag tag) {
        if (std::string{tag.data(), tag.size()} == active_version){
            Network::send_message(new char []("Not OTA updating to equal version"));
        }

        // pause time sensitive temperature
        ok_to_rmt_read = false;
        vTaskDelay(pdMS_TO_TICKS(1000));

        const esp_partition_t* running_part = esp_ota_get_running_partition();
        esp_app_desc_t running_description;
        esp_err_t err = esp_ota_get_partition_description(running_part, &running_description);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to get information about currently running app");
        } else {
            ESP_LOGI(TAG, "========   CURRENT PROJECT   ========");
            ESP_LOGI(TAG, "Project: %.32s ", running_description.project_name);
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

        err = esp_ota_begin(active_ota_part, OTA_WITH_SEQUENTIAL_WRITES, &ota_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start OTA: %s", esp_err_to_name(err));
            return;
        }
#ifdef DEV_SERVER
        std::string ota_url = "http://";
        ota_url += DEV_SERVER;
        ota_url += ":3000/api/files/ota/" + std::string(tag.data(), tag.size()) + "/Core.bin";
#else
        std::string ota_url = "http://";
        ota_url += Storage::get_server();
        ota_url += "/api/files/ota/" + std::string(tag.data(), tag.size()) + "/Core.bin";
#endif

        std::string* ptr_url = new std::string(ota_url);
        next_version = ">" + std::string{tag.data(), tag.size()};
                    

        HTTPManager::Transfer xfer = {
            .type = HTTPManager::OperationType::GET,
            .start =
                [](void* vp_url, const char** url) {
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
                [](void* vp_url, esp_err_t err) {
                    ok_to_rmt_read = true;

                    if (err != ESP_OK) {
                        next_version = "!" + next_version; // will show !> version to show error on pending version
                        std::string smsg = std::string("Failed to download OTA update: ") + esp_err_to_name(err);
                        ESP_LOGE(TAG, "%s", smsg.c_str());
                        char* msg = new char[smsg.size()];
                        strcpy(msg, smsg.data());
                        Network::send_message(msg);
                        return;
                    }
                    err = esp_ota_end(ota_handle);
                    if (err != ESP_OK) {
                        next_version = "!" + next_version; // will show !> version to show error on pending version
                        std::string smsg = std::string("Failed to install OTA update: ") + esp_err_to_name(err);
                        ESP_LOGE(TAG, "%s", smsg.c_str());
                        char* msg = new char[smsg.size()];
                        strcpy(msg, smsg.data());
                        Network::send_message(msg);
                        return;
                    }
                    const esp_partition_t* running_part = esp_ota_get_running_partition();
                    const esp_partition_t* active_ota_part = esp_ota_get_next_update_partition(running_part);
                    esp_app_desc_t new_desc = {0};
                    err = esp_ota_get_partition_description(active_ota_part, &new_desc);
                    if (err != ESP_OK) {
                        ESP_LOGW(TAG, "Failed to get ota partition description");
                    } else {
                        ESP_LOGI(TAG, "========   UPDATED PROJECT   ========");
                        ESP_LOGI(TAG, "Project: %.32s ", new_desc.project_name);
                        ESP_LOGI(TAG, "Version: %.32s", new_desc.version);
                        ESP_LOGI(TAG, "%.16s %.16s", new_desc.date, new_desc.time);
                        ESP_LOGI(TAG, "ESP IDF Version %.32s", new_desc.idf_ver);
                        next_version = new_desc.version;
                    }

                    esp_ota_set_boot_partition(esp_ota_get_next_update_partition(NULL));
                    delete (std::string*)(vp_url);
                },
            .user_data = (void*)ptr_url,
        };
        HTTPManager::queue_transfer(xfer);
    }
    void mark_valid() {
        esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
        if (err != ESP_OK) {
            ESP_LOGW(TAG, "Unable to mark app as valid?: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "Marked OTA as valid");
            Network::send_message(new char[]("Successfully reached server. Marking OTA valid"));
        }
    }

    const char* image_state_to_str(esp_ota_img_states_t state) {
        switch (state) {
            case ESP_OTA_IMG_NEW:
                return "NEW";
                break;
            case ESP_OTA_IMG_PENDING_VERIFY:
                return "PENDING_VERIFY";
                break;
            case ESP_OTA_IMG_VALID:
                return "VALID";
                break;
            case ESP_OTA_IMG_INVALID:
                return "INVALID";
                break;
            case ESP_OTA_IMG_ABORTED:
                return "ABORTED";
                break;
            default:
                return "UNDEFINED";
                break;
        }
    }

    void init() {
        ESP_LOGI(TAG, "========   CURRENT PROJECT   ========");
        const esp_partition_t* running_part = esp_ota_get_running_partition();
        esp_app_desc_t running_description;
        esp_err_t err = esp_ota_get_partition_description(running_part, &running_description);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "  Failed to get information about app");
        } else {
            ESP_LOGI(TAG, "  Project: %.32s ", running_description.project_name);
            ESP_LOGI(TAG, "  Version: %.32s", running_description.version);
            ESP_LOGI(TAG, "  %.16s %.16s", running_description.date, running_description.time);
            ESP_LOGI(TAG, "  ESP IDF Version %.32s", running_description.idf_ver);
            active_version = std::string{running_description.version};
        }
        esp_ota_img_states_t state;
        err = esp_ota_get_state_partition(running_part, &state);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "  Failed to get state of app");
        } else {
            ESP_LOGI(TAG, "  Image State: %s", image_state_to_str(state));
        }
        ESP_LOGI(TAG, "=====================================");
    }

} // namespace OTA