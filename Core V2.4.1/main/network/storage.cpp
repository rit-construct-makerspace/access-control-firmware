#include "storage.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

namespace Storage {
    static const char* TAG = "storage";

    void update_bootcount() {
        nvs_handle_t my_handle;
        esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "(%s) opening NVS handle!\n", esp_err_to_name(err));
        } else {
            printf("Done\n");
        }

        // Read
        // value will default to 0, if not set yet in NVS
        int32_t restart_counter = 0;

        err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
        switch (err) {
        case ESP_OK:
            ESP_LOGI(TAG, "Restart counter = %ld\n", restart_counter);
            break;
        case ESP_ERR_NVS_NOT_FOUND:
            break;
        default:
            printf("Error (%s) reading!\n", esp_err_to_name(err));
        }

        // Write
        printf("Updating restart counter in NVS ... ");
        restart_counter++;
        err = nvs_set_i32(my_handle, "restart_counter", restart_counter);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set restart count");
        }

        // Commit written value.
        // After setting any values, nvs_commit() must be called to ensure
        // changes are written to flash storage. Implementations may write
        // to storage at other times, but this is not guaranteed.
        err = nvs_commit(my_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to commit to NVS");
        }

        // Close
        nvs_close(my_handle);
    }

    int init() {
        // Initialize NVS
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES ||
            err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
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

std::string get_network_ssid() {
    std::string returnValue = "RIT-WiFi";
    return returnValue;
}

std::string get_network_password() {
    std::string returnValue = "";
    return returnValue;
}

std::string get_server() {
    std::string returnValue = "calcarea.student.rit.edu";
    return returnValue;
}

std::string get_server_certs() {
    std::string returnValue = "";
    return returnValue;
}

std::string get_serial_number() {
    std::string returnValue = "1234567890";
    return returnValue;
}

std::string get_key() {
    std::string returnValue = "ABCDEFGHIJ";
    return returnValue;
}

bool set_network_ssid(std::string ssid) { return false; }

bool set_network_password(std::string password) { return false; }

bool set_server(std::string server) { return false; }

} // namespace Storage