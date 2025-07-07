#include "storage.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

namespace Storage {
    static const char* TAG = "storage";

    esp_err_t update_bootcount() {
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "(%s) opening NVS handle!", esp_err_to_name(err));
        }

        // Read
        int32_t restart_counter = 0;

        err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
        switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Restart counter = %ld", restart_counter);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            break;
        default:
            ESP_LOGE(TAG, "(%s) reading!", esp_err_to_name(err));
        }

        // Write
        restart_counter++;
        ESP_ERROR_CHECK_WITHOUT_ABORT(
            nvs_set_i32(my_handle, "restart_counter", restart_counter));
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(my_handle));

        // Close
        nvs_close(my_handle);
        return ESP_OK;
    }

    int init() {
        // Initialize NVS
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGI(TAG, "Having to reset NVS (evil and bad)");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }
        ESP_ERROR_CHECK(err);
        if (err != ESP_OK) {
            return 1;
        }

        update_bootcount();
        return 0;
    }

    WifiSSID get_network_ssid() {
        WifiSSID ssid = {'R', 'I', 'T', '-', 'W', 'i', 'F', 'i', 0x0};
        return ssid;
    }

    WifiPassword get_network_password() {
        WifiPassword pass = {0x0};
        return pass;
    }

    std::string get_server() {
        std::string returnValue = "make.rit.edu";
        return returnValue;
    }

    std::string get_server_certs() {
        std::string returnValue = "";
        return returnValue;
    }

    std::string get_serial_number() {
        std::string returnValue = "1234";
        return returnValue;
    }

    std::string get_key() {
        std::string returnValue = "8846c500a8b229aa1b5de8878dfd400b";
        return returnValue;
    }

    bool set_network_ssid(std::string ssid) { return false; }

    bool set_network_password(std::string password) { return false; }

    bool set_server(std::string server) { return false; }

} // namespace Storage