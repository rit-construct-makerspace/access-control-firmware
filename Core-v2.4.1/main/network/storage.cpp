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
               "MIIFBjCCAu6gAwIBAgIRAMISMktwqbSRcdxA9+KFJjwwDQYJKoZIhvcNAQELBQAw\n"
               "TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
               "cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMjQwMzEzMDAwMDAw\n"
               "WhcNMjcwMzEyMjM1OTU5WjAzMQswCQYDVQQGEwJVUzEWMBQGA1UEChMNTGV0J3Mg\n"
               "RW5jcnlwdDEMMAoGA1UEAxMDUjEyMIIBIjANBgkqhkiG9w0BAQEFAAOCAQ8AMIIB\n"
               "CgKCAQEA2pgodK2+lP474B7i5Ut1qywSf+2nAzJ+Npfs6DGPpRONC5kuHs0BUT1M\n"
               "5ShuCVUxqqUiXXL0LQfCTUA83wEjuXg39RplMjTmhnGdBO+ECFu9AhqZ66YBAJpz\n"
               "kG2Pogeg0JfT2kVhgTU9FPnEwF9q3AuWGrCf4yrqvSrWmMebcas7dA8827JgvlpL\n"
               "Thjp2ypzXIlhZZ7+7Tymy05v5J75AEaz/xlNKmOzjmbGGIVwx1Blbzt05UiDDwhY\n"
               "XS0jnV6j/ujbAKHS9OMZTfLuevYnnuXNnC2i8n+cF63vEzc50bTILEHWhsDp7CH4\n"
               "WRt/uTp8n1wBnWIEwii9Cq08yhDsGwIDAQABo4H4MIH1MA4GA1UdDwEB/wQEAwIB\n"
               "hjAdBgNVHSUEFjAUBggrBgEFBQcDAgYIKwYBBQUHAwEwEgYDVR0TAQH/BAgwBgEB\n"
               "/wIBADAdBgNVHQ4EFgQUALUp8i2ObzHom0yteD763OkM0dIwHwYDVR0jBBgwFoAU\n"
               "ebRZ5nu25eQBc4AIiMgaWPbpm24wMgYIKwYBBQUHAQEEJjAkMCIGCCsGAQUFBzAC\n"
               "hhZodHRwOi8veDEuaS5sZW5jci5vcmcvMBMGA1UdIAQMMAowCAYGZ4EMAQIBMCcG\n"
               "A1UdHwQgMB4wHKAaoBiGFmh0dHA6Ly94MS5jLmxlbmNyLm9yZy8wDQYJKoZIhvcN\n"
               "AQELBQADggIBAI910AnPanZIZTKS3rVEyIV29BWEjAK/duuz8eL5boSoVpHhkkv3\n"
               "4eoAeEiPdZLj5EZ7G2ArIK+gzhTlRQ1q4FKGpPPaFBSpqV/xbUb5UlAXQOnkHn3m\n"
               "FVj+qYv87/WeY+Bm4sN3Ox8BhyaU7UAQ3LeZ7N1X01xxQe4wIAAE3JVLUCiHmZL+\n"
               "qoCUtgYIFPgcg350QMUIWgxPXNGEncT921ne7nluI02V8pLUmClqXOsCwULw+PVO\n"
               "ZCB7qOMxxMBoCUeL2Ll4oMpOSr5pJCpLN3tRA2s6P1KLs9TSrVhOk+7LX28NMUlI\n"
               "usQ/nxLJID0RhAeFtPjyOCOscQBA53+NRjSCak7P4A5jX7ppmkcJECL+S0i3kXVU\n"
               "y5Me5BbrU8973jZNv/ax6+ZK6TM8jWmimL6of6OrX7ZU6E2WqazzsFrLG3o2kySb\n"
               "zlhSgJ81Cl4tv3SbYiYXnJExKQvzf83DYotox3f0fwv7xln1A2ZLplCb0O+l/AK0\n"
               "YE0DS2FPxSAHi0iwMfW2nNHJrXcY3LLHD77gRgje4Eveubi2xxa+Nmk/hmhLdIET\n"
               "iVDFanoCrMVIpQ59XWHkzdFmoHXHBV7oibVjGSO7ULSQ7MJ1Nz51phuDJSgAIU7A\n"
               "0zrLnOrAj/dfrlEWRhCvAgbuwLZX1A2sjNjXoPOHbsPiy+lO1KF8/XY7\n"
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