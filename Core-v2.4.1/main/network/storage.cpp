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
    bool commit() {
        esp_err_t e = nvs_commit(storage_nvs_handle);
        if (e != ESP_OK) {
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
               "MIIE9TCCA92gAwIBAgISBZCQYqbHFSFCDWH3Qr3euLVzMA0GCSqGSIb3DQEBCwUA\n"
               "MDMxCzAJBgNVBAYTAlVTMRYwFAYDVQQKEw1MZXQncyBFbmNyeXB0MQwwCgYDVQQD\n"
               "EwNSMTIwHhcNMjYwMjEyMTgzMDE5WhcNMjYwNTEzMTgzMDE4WjAXMRUwEwYDVQQD\n"
               "EwxtYWtlLnJpdC5lZHUwggEiMA0GCSqGSIb3DQEBAQUAA4IBDwAwggEKAoIBAQDl\n"
               "8zpLrvXsjt3NBVnFw1klQ2dkJUSPMDmVe2/IQzrUkECY+5PX70xgkyJMkvCoNLyx\n"
               "RHDoDszOab9za5Carx6b1Fn+gNKaGt7/rxlz7glFiYiQzGX9wJYA7svaULlc49QS\n"
               "opXejsM5rcuReTREQ6cBQd1fhg/zvqtzcfFpqrV1WZz7g/5JcKFC69ldH8fLfiPp\n"
               "K7w7SZyO4IoH2FRe3Zc9EaZAFsSJMSBhkwwTGDP+F8t0JMMbqkqI9JwPfEv7Y4uy\n"
               "erVx7b9eiYSaa0e0KdAhYtpDBWOKAINarMxS/NpArZNJT7asV/MMFzIi+3pWbzAd\n"
               "16FSWA5lOkA7+nnUgKIdAgMBAAGjggIdMIICGTAOBgNVHQ8BAf8EBAMCBaAwEwYD\n"
               "VR0lBAwwCgYIKwYBBQUHAwEwDAYDVR0TAQH/BAIwADAdBgNVHQ4EFgQUotMvPboR\n"
               "Y+rJLmoqIzdeEcEkRUwwHwYDVR0jBBgwFoAUALUp8i2ObzHom0yteD763OkM0dIw\n"
               "MwYIKwYBBQUHAQEEJzAlMCMGCCsGAQUFBzAChhdodHRwOi8vcjEyLmkubGVuY3Iu\n"
               "b3JnLzAXBgNVHREEEDAOggxtYWtlLnJpdC5lZHUwEwYDVR0gBAwwCjAIBgZngQwB\n"
               "AgEwLwYDVR0fBCgwJjAkoCKgIIYeaHR0cDovL3IxMi5jLmxlbmNyLm9yZy8xMTYu\n"
               "Y3JsMIIBDgYKKwYBBAHWeQIEAgSB/wSB/AD6AHcASZybad4dfOz8Nt7Nh2SmuFuv\n"
               "CoeAGdFVUvvp6ynd+MMAAAGcU1PWAQAABAMASDBGAiEArIeuDoqiTazmCVKYWnI4\n"
               "siWkISN1pDKZGTDncZqpQ7kCIQC0O0Vs2vE1HpuiSX61vaXB1HVxUwLBYDdYgIs6\n"
               "od8cjwB/AOMjjfKNoojgquCs8PqQyYXwtr/10qUnsAH8HERYxLboAAABnFNT13MA\n"
               "CAAABQAyYCJNBAMASDBGAiEA4eUzyJ8NZHYSDvQtRYA/gN3wcfYD19KDc9kzO8yP\n"
               "JRwCIQCcDe9Q8ra3u9bY/caYhCJq4Nx9Eaa0u+TJarGxaJ8nZTANBgkqhkiG9w0B\n"
               "AQsFAAOCAQEABXs2m5lMhePahwx49lzrkoHh8Rii8Wj89bqctE+LPMojI8mewjgt\n"
               "O0YUTTZ9gyu08HeyI9xKffojyAfRgtaAgkgtVFSGkp8YJzlu8TRdhp/3/wW9hgsX\n"
               "+oYpGtV/sXZg++IW4Wji5HsJEL/QreArMMnTF54PUKtNuVvHkT0fCIUf2UyvrU8F\n"
               "BT4qmFg3v15TfgimKJrXQfcEjvXo7WNw3WgkTzdSYHe3Ood0NICo7rK9JgIzlM/L\n"
               "8JwQ8g2ayGTaFV1uTOJp4rjg7YSH7bQD3XLnk/NYrB0NMbZdKoyBE8hCQy/+bnjt\n"
               "6iwbf/qNUCdlHqKT6DYyMNgvram6t3o2mw==\n"
               "-----END CERTIFICATE-----\n";
    }

    std::string get_key() {
#ifdef DEV_SERVER
        return "07edfd78f2a97d0d2c46c1cb4504fbe343a9bb6ec7f2a64b41d2c7d4f6fcca7f63f78220b70230e3f022e395fe0eb436";
#else
        return cached_server_key;
#endif
    }

    uint8_t get_max_temp() {
        return cached_max_temp;
    }

    bool set_network_ssid(WifiSSID ssid) {
        esp_err_t err = nvs_set_blob(storage_nvs_handle, NVS_NETWORK_SSID_TAG, ssid.data(), sizeof(ssid));
        if (err != ESP_OK) {
            return false;
        }
        bool ok = commit();
        if (ok) {
            cached_network_ssid = ssid;
        }
        return ok;
    }

    bool set_network_password(WifiPassword password) {
        esp_err_t err = nvs_set_blob(storage_nvs_handle, NVS_NETWORK_PASS_TAG, password.data(), sizeof(password));
        if (err != ESP_OK) {
            return false;
        }
        bool ok = commit();
        if (ok) {
            cached_network_pass = password;
        }
        return ok;
    }

    bool set_server(std::string server) {
        esp_err_t err = nvs_set_str(storage_nvs_handle, NVS_SERVER_ADDR_TAG, server.c_str());
        if (err != ESP_OK) {
            return false;
        }
        bool ok = commit();
        if (ok) {
            cached_server_addr = server;
        }
        return ok;
    }
    bool set_key(std::string key) {
        esp_err_t err = nvs_set_str(storage_nvs_handle, NVS_SERVER_KEY_TAG, key.c_str());
        if (err != ESP_OK) {
            return false;
        }
        bool ok = commit();
        if (ok) {
            cached_server_key = key;
        }
        return ok;
    }

    bool set_max_temp(uint8_t max_temp) {
        esp_err_t err = nvs_set_u8(storage_nvs_handle, NVS_MAX_TEMP_TAG, max_temp);
        if (err != ESP_OK) {
            return false;
        }
        bool ok = commit();
        if (ok) {
            cached_max_temp = max_temp;
        }
        return ok;
    }

} // namespace Storage