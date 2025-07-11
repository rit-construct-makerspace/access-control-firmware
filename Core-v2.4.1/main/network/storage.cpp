#include "storage.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "common/pins.hpp"

namespace Storage {
    static constexpr const char* NVS_SERVER_ADDR_TAG = "server_addr";
    static constexpr const char* NVS_SERVER_KEY_TAG = "server_key";

    static constexpr const char* NVS_NETWORK_SSID_TAG = "network_ssid";
    static constexpr const char* NVS_NETWORK_PASS_TAG = "network_pass";

    static constexpr const char* NVS_MAX_TEMP_TAG = "max_temp";

    static const char* TAG = "storage";
    nvs_handle_t storage_nvs_handle;

    WifiSSID cached_network_ssid = {0};
    WifiPassword cached_network_pass = {0};

    std::string cached_server_key = "";
    std::string cached_server_addr = "";

    uint8_t cached_max_temp = 40;

    esp_err_t update_bootcount() {

        // Read
        int32_t boot_counter = 0;

        esp_err_t err = nvs_get_i32(storage_nvs_handle, "boot_counter", &boot_counter);
        switch (err) {
            case ESP_OK:
                ESP_LOGI(TAG, "Boot counter = %ld", boot_counter);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                ESP_LOGI(TAG, "First Boot!");
                break;
            default:
                ESP_LOGE(TAG, "(%s) reading!", esp_err_to_name(err));
        }

        // Write
        boot_counter++;
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_set_i32(storage_nvs_handle, "boot_counter", boot_counter));
        ESP_ERROR_CHECK_WITHOUT_ABORT(nvs_commit(storage_nvs_handle));

        return ESP_OK;
    }
    bool commit(){
        esp_err_t e = nvs_commit(storage_nvs_handle);
        if (e != ESP_OK){
            ESP_LOGE(TAG, "Could not commit to NVS: %s", esp_err_to_name(e));
        }
        return e == ESP_OK;
    }

    void load_initial_values() {
        size_t len = sizeof(WifiSSID);

        esp_err_t err = nvs_get_blob(storage_nvs_handle, NVS_NETWORK_SSID_TAG, cached_network_ssid.data(), &len);
        if (len != sizeof(WifiSSID) || err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load network SSID from NVS: %s", esp_err_to_name(err));
        }
        len = sizeof(WifiPassword);
        err = nvs_get_blob(storage_nvs_handle, NVS_NETWORK_PASS_TAG, cached_network_pass.data(), &len);
        if (len != sizeof(WifiPassword) || err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load network password from NVS: %s", esp_err_to_name(err));
        }
        static char reading_buf[512] = {0};
        len = sizeof(reading_buf);
        err = nvs_get_str(storage_nvs_handle, NVS_SERVER_ADDR_TAG, reading_buf, &len);
        if (len == 0 || err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load server address from NVS: %s", esp_err_to_name(err));
        } else {
            cached_server_addr = reading_buf;
        }
        len = sizeof(reading_buf);
        err = nvs_get_str(storage_nvs_handle, NVS_SERVER_KEY_TAG, reading_buf, &len);
        if (len == 0 || err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load server key from NVS: %s", esp_err_to_name(err));
        } else {
            cached_server_key = reading_buf;
        }

        uint8_t temp_int;
        err = nvs_get_u8(storage_nvs_handle, NVS_MAX_TEMP_TAG, &temp_int);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to load max temp from NVS: %s", esp_err_to_name(err));
        } else {
            cached_max_temp = temp_int;
        }
    }

    int init() {
        // Initialize NVS
        esp_err_t err = nvs_flash_init();
        if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
            ESP_LOGI(TAG, "Having to reset NVS (evil and bad)");
            ESP_ERROR_CHECK(nvs_flash_erase());
            err = nvs_flash_init();
        }

        ESP_ERROR_CHECK(err);
        if (err != ESP_OK) {
            return 1;
        }
        err = nvs_open("storage", NVS_READWRITE, &storage_nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "(%s) opening NVS handle!", esp_err_to_name(err));
        }

        update_bootcount();
        load_initial_values();

        return 0;
    }

    WifiSSID get_network_ssid() {
        return cached_network_ssid;
    }

    WifiPassword get_network_password() {
        return cached_network_pass;
    }

    std::string get_server() {
        return cached_server_addr;
    }

    const char* get_server_certs() {
        return "-----BEGIN CERTIFICATE-----\n"
               "MIIGSjCCBDKgAwIBAgIRAINbdhUgbS1uCX4LbkCf78AwDQYJKoZIhvcNAQEMBQAw\n"
               "gYgxCzAJBgNVBAYTAlVTMRMwEQYDVQQIEwpOZXcgSmVyc2V5MRQwEgYDVQQHEwtK\n"
               "ZXJzZXkgQ2l0eTEeMBwGA1UEChMVVGhlIFVTRVJUUlVTVCBOZXR3b3JrMS4wLAYD\n"
               "VQQDEyVVU0VSVHJ1c3QgUlNBIENlcnRpZmljYXRpb24gQXV0aG9yaXR5MB4XDTIy\n"
               "MTExNjAwMDAwMFoXDTMyMTExNTIzNTk1OVowRDELMAkGA1UEBhMCVVMxEjAQBgNV\n"
               "BAoTCUludGVybmV0MjEhMB8GA1UEAxMYSW5Db21tb24gUlNBIFNlcnZlciBDQSAy\n"
               "MIIBojANBgkqhkiG9w0BAQEFAAOCAY8AMIIBigKCAYEAifBcxDi60DRXr5dVoPQi\n"
               "Q/w+GBE62216UiEGMdbUt7eSiIaFj/iZ/xiFop0rWuH4BCFJ3kSvQF+aIhEsOnuX\n"
               "R6mViSpUx53HM5ApIzFIVbd4GqY6tgwaPzu/XRI/4Dmz+hoLW/i/zD19iXvS95qf\n"
               "NU8qP7/3/USf2/VNSUNmuMKlaRgwkouue0usidYK7V8W3ze+rTFvWR2JtWKNTInc\n"
               "NyWD3GhVy/7G09PwTAu7h0qqRyTkETLf+z7FWtc8c12f+SfvmKHKFVqKpNPtgMkr\n"
               "wqwaOgOOD4Q00AihVT+UzJ6MmhNPGg+/Xf0BavmXKCGDTv5uzQeOdD35o/Zw16V4\n"
               "C4J4toj1WLY7hkVhrzKG+UWJiSn8Hv3dUTj4dkneJBNQrUfcIfTHV3gCtKwXn1eX\n"
               "mrxhH+tWu9RVwsDegRG0s28OMdVeOwljZvYrUjRomutNO5GzynveVxJVCn3Cbn7a\n"
               "c4L+5vwPNgs04DdOAGzNYdG5t6ryyYPosSLH2B8qDNzxAgMBAAGjggFwMIIBbDAf\n"
               "BgNVHSMEGDAWgBRTeb9aqitKz1SA4dibwJ3ysgNmyzAdBgNVHQ4EFgQU70wAkqb7\n"
               "di5eleLJX4cbGdVN4tkwDgYDVR0PAQH/BAQDAgGGMBIGA1UdEwEB/wQIMAYBAf8C\n"
               "AQAwHQYDVR0lBBYwFAYIKwYBBQUHAwEGCCsGAQUFBwMCMCIGA1UdIAQbMBkwDQYL\n"
               "KwYBBAGyMQECAmcwCAYGZ4EMAQICMFAGA1UdHwRJMEcwRaBDoEGGP2h0dHA6Ly9j\n"
               "cmwudXNlcnRydXN0LmNvbS9VU0VSVHJ1c3RSU0FDZXJ0aWZpY2F0aW9uQXV0aG9y\n"
               "aXR5LmNybDBxBggrBgEFBQcBAQRlMGMwOgYIKwYBBQUHMAKGLmh0dHA6Ly9jcnQu\n"
               "dXNlcnRydXN0LmNvbS9VU0VSVHJ1c3RSU0FBQUFDQS5jcnQwJQYIKwYBBQUHMAGG\n"
               "GWh0dHA6Ly9vY3NwLnVzZXJ0cnVzdC5jb20wDQYJKoZIhvcNAQEMBQADggIBACaA\n"
               "DTTkHq4ivq8+puKE+ca3JbH32y+odcJqgqzDts5bgsapBswRYypjmXLel11Q2U6w\n"
               "rySldlIjBRDZ8Ah8NOs85A6MKJQLaU9qHzRyG6w2UQTzRwx2seY30Mks3ZdIe9rj\n"
               "s5rEYliIOh9Dwy8wUTJxXzmYf/A1Gkp4JJp0xIhCVR1gCSOX5JW6185kwid242bs\n"
               "Lm0vCQBAA/rQgxvLpItZhC9US/r33lgtX/cYFzB4jGOd+Xs2sEAUlGyu8grLohYh\n"
               "kgWN6hqyoFdOpmrl8yu7CSGV7gmVQf9viwVBDIKm+2zLDo/nhRkk8xA0Bb1BqPzy\n"
               "bPESSVh4y5rZ5bzB4Lo2YN061HV9+HDnnIDBffNIicACdv4JGyGfpbS6xsi3UCN1\n"
               "5ypaG43PJqQ0UnBQDuR60io1ApeSNkYhkaHQ9Tk/0C4A+EM3MW/KFuU53eHLVlX9\n"
               "ss1iG2AJfVktaZ2l/SbY7py8JUYMkL/jqZBRjNkD6srsmpJ6utUMmAlt7m1+cTX8\n"
               "6/VEBc5Dp9VfuD6hNbNKDSg7YxyEVaBqBEtN5dppj4xSiCrs6LxLHnNo3rG8VJRf\n"
               "NVQdgFbMb7dOIBokklzfmU69lS0kgyz2mZMJmW2G/hhEdddJWHh3FcLi2MaeYiOV\n"
               "RFrLHtJvXEdf2aEaZ0LOb2Xo3zO6BJvjXldv2woN\n"
               "-----END CERTIFICATE-----\n";
    }

    std::string get_key() {
        return cached_server_key;
    }

    uint8_t get_max_temp() {
        return cached_max_temp;
    }

    bool set_network_ssid(WifiSSID ssid) {
        esp_err_t err = nvs_set_blob(storage_nvs_handle, NVS_NETWORK_SSID_TAG, ssid.data(), sizeof(ssid));
        if (err == ESP_OK) {
            cached_network_ssid = ssid;
            return true;
        }
        return commit();
    }

    bool set_network_password(WifiPassword password) {
        esp_err_t err = nvs_set_blob(storage_nvs_handle, NVS_NETWORK_PASS_TAG, password.data(), sizeof(password));
        if (err == ESP_OK) {
            cached_network_pass = password;
            return true;
        }
        return commit();
    }

    bool set_server(std::string server) {
        esp_err_t err = nvs_set_str(storage_nvs_handle, NVS_SERVER_ADDR_TAG, server.c_str());

        if (err == ESP_OK) {
            cached_server_addr = server;
            return true;
        }
        return commit();
    }
    bool set_key(std::string key) {
        esp_err_t err = nvs_set_str(storage_nvs_handle, NVS_SERVER_KEY_TAG, key.c_str());

        if (err == ESP_OK) {
            cached_server_key = key;
            return true;
        }
        return commit();
    }

    bool set_max_temp(uint8_t max_temp) {
        esp_err_t err = nvs_set_u8(storage_nvs_handle, NVS_MAX_TEMP_TAG, max_temp);

        if (err == ESP_OK) {
            cached_max_temp = max_temp;
            return true;
        }

        return commit();
    }

} // namespace Storage