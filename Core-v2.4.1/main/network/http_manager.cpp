#include "http_manager.hpp"
#include "esp_http_client.h"
#include "esp_log.h"

#include "common/hardware.hpp"
#include "storage.hpp"

namespace HTTPManager {
    static const char* TAG = "http-man";
    esp_err_t _http_event_handle(esp_http_client_event_t* evt) {
        switch (evt->event_id) {
            case HTTP_EVENT_ERROR:
                ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
                break;
            case HTTP_EVENT_ON_CONNECTED:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
                break;
            case HTTP_EVENT_HEADER_SENT:
                ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
                break;
            case HTTP_EVENT_ON_HEADER:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
                ESP_LOGI(TAG, "%.*s", evt->data_len, (char*)evt->data);
                break;
            case HTTP_EVENT_ON_DATA:
                ESP_LOGI("main", "Free heap %lu", esp_get_free_heap_size());
                ESP_LOGI("main", "Free heap %lu", esp_get_free_heap_size());
                ESP_LOGI(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
                if (!esp_http_client_is_chunked_response(evt->client)) {
                    ESP_LOGI(TAG, "%.*s", evt->data_len, (char*)evt->data);
                }

                break;
            case HTTP_EVENT_ON_FINISH:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
                break;
            case HTTP_EVENT_DISCONNECTED:
                ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
                break;
            default:
                ESP_LOGE(TAG, "UNKNOWN EVENT ID FOR HTTTPS");
                break;
        }
        return ESP_OK;
    }

    void init(const char* server) {

        esp_http_client_config_t config = {
            .url = "http://calcarea.student.rit.edu:3000/api/files/certCA",
            // .cert_pem = Storage::get_server_certs(),
            // .cert_len = 0, // will strlen it
            // .port = 3000,
            .event_handler = _http_event_handle,
            // .transport_type
        };
        esp_http_client_handle_t client = esp_http_client_init(&config);

        esp_http_client_set_header(client, "shlug-sn", Hardware::get_serial_number());
        std::string key = Storage::get_key();
        
        esp_http_client_set_header(client, "shlug-key", "e69bfc84c69a6c08d85757c6b8bb6ff6048fa808fe53ed598ab6a9536e559cbd9568ce2ce03fbee7c5b95c1529987c14");
        // esp_http_client_set_header(client, "shlug-key", key.c_str());

        esp_http_client_set_method(client, HTTP_METHOD_GET);

        esp_err_t err = esp_http_client_perform(client);

        if (err == ESP_OK) {
            ESP_LOGI(TAG, "Status = %d, content_length = %d", (int)esp_http_client_get_status_code(client),
                     (int)esp_http_client_get_content_length(client));
        }
        esp_http_client_cleanup(client);
    }

} // namespace HTTPManager