#include "http_manager.hpp"
#include "common/hardware.hpp"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "storage.hpp"

bool ok_to_rmt_read = true;

namespace HTTPManager {
    static constexpr size_t HTTP_BLOCK_SIZE = 512;
    enum control_message {
        Start,
        Finished,
    };

    static const char* TAG = "http-man";

    static TaskHandle_t http_thread = NULL;
    static TaskHandle_t performer_thread = NULL;
    static QueueHandle_t transfer_request_queue = NULL;
    static QueueHandle_t http_control_queue = NULL;
    static StreamBufferHandle_t http_data_buf = NULL;

    static esp_http_client_handle_t client = NULL;

    esp_err_t _http_event_handle(esp_http_client_event_t* evt) {
        static int recv = 0;

        const control_message start = control_message::Start;
        switch (evt->event_id) {
            case HTTP_EVENT_ERROR:
                ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
                break;
            case HTTP_EVENT_ON_CONNECTED:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
                xQueueSend(http_control_queue, &start, pdMS_TO_TICKS(100));
                break;
            case HTTP_EVENT_HEADER_SENT:
                ESP_LOGI(TAG, "HTTP_EVENT_HEADER_SENT");
                break;
            case HTTP_EVENT_ON_HEADER:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_HEADER");
                ESP_LOGI(TAG, "%s:%s", evt->header_key, evt->header_value);
                recv = 0;
                break;
            case HTTP_EVENT_ON_DATA:
                if (!esp_http_client_is_chunked_response(evt->client)) {
                    int sent = xStreamBufferSend(http_data_buf, evt->data, evt->data_len, portMAX_DELAY);
                    recv += evt->data_len;
                }
                break;
            case HTTP_EVENT_ON_FINISH:
                ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
                /* falls through*/
            case HTTP_EVENT_DISCONNECTED: {
                ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
                control_message cm = control_message::Finished;
                xQueueSend(http_control_queue, &cm, pdMS_TO_TICKS(1));
            } break;
            default:
                ESP_LOGE(TAG, "UNKNOWN EVENT ID FOR HTTTPS");
                break;
        }
        return ESP_OK;
    }

    QueueHandle_t perf_q = NULL;
    bool start_performing() {
        int val = 1;
        return xQueueSend(perf_q, (void*)&val, pdMS_TO_TICKS(1000)) == pdTRUE;
    }

    esp_err_t execute_get(Transfer xfer) {
        const char* url = NULL;
        esp_err_t err = xfer.start(xfer.user_data, &url);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start transfer: %s", esp_err_to_name(err));
            return err;
        } else {
            ESP_LOGI(TAG, "Beginning transfer with URL: %s and client %p", url ? url : "NULL URL", client);
        }
        err = esp_http_client_set_url(client, url);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to set http client URL: %s", esp_err_to_name(err));
            return err;
        }
        esp_http_client_set_method(client, HTTP_METHOD_GET);
        if (!start_performing()) {
            ESP_LOGE(TAG, "Couldnt start http processing");
            return ESP_FAIL;
        }

        static control_message cm = control_message::Finished;
        for (int attempts = 0; attempts < 10; attempts++) {
            if (xQueueReceive(http_control_queue, &cm, pdMS_TO_TICKS(500)) != pdTRUE) {
                continue;
            }
            if (cm == control_message::Start) {
                break;
            }
        }
        if (cm != Start) {
            ESP_LOGE(TAG, "Never connected to server, cant download file");
            return ESP_FAIL;
        }

        ESP_LOGI(TAG, "Starting to listen");
        while (true) {
            static uint8_t buf[4096] = {0};
            if (xQueueReceive(http_control_queue, &cm, pdMS_TO_TICKS(1)) == pdTRUE) {
                ESP_LOGW(TAG, "Received stop message from queue");
                // finished
                break;
            }
            size_t read = xStreamBufferReceive(http_data_buf, buf, 4096, pdMS_TO_TICKS(500));
            // ESP_LOGI(TAG, "REad %d bs", (int)read);
            esp_err_t err = xfer.data(xfer.user_data, buf, &read);
            if (err != ESP_OK) {
                // bail early, will call finish with error for clean up
                return err;
            }
        }
        return ESP_OK;
    }

    void http_performer(void*) {
        while (true) {
            int i = 0;
            if (xQueueReceive(perf_q, &i, portMAX_DELAY) != pdTRUE) {
                continue;
            }
            esp_http_client_perform(client);
        }
    }
    void thread_fn(void*) {
        while (true) {
            Transfer xfer = {};
            if (xQueueReceive(transfer_request_queue, (void*)&xfer, portMAX_DELAY) != pdTRUE) {
                // noting asked for
                continue;
            };

            if (xfer.type == OperationType::GET) {
                xQueueReset(http_control_queue);
                xStreamBufferReset(http_data_buf);
                esp_err_t err = execute_get(xfer);
                if (err != ESP_OK) {
                    ESP_LOGI(TAG, "Stopped transfer due to execute error");
                }
                xfer.finish(xfer.user_data, err);
                ok_to_rmt_read = true;
            }
        }
    }

    bool queue_transfer(Transfer xfer) {
        if (xfer.start == NULL || xfer.data == NULL || xfer.finish == NULL) {
            ESP_LOGE(TAG, "Not Submitting transfer request due to null callbacks");
            return false;
        }

        return xQueueSend(transfer_request_queue, &xfer, pdMS_TO_TICKS(100)) == pdTRUE;
    }

    void init() {

        esp_http_client_config_t config = {
            .url = "https://make.rit.edu/", // will be reset to actual target before any actual requests
#ifndef DEV_SERVER
            .cert_pem = Storage::get_server_certs(),
            .cert_len = 0, // will strlen it
#endif
            .event_handler = _http_event_handle,
        };
        client = esp_http_client_init(&config);

        esp_http_client_set_header(client, "shlug-sn", Hardware::get_serial_number());
        std::string key = Storage::get_key();
        esp_http_client_set_header(client, "shlug-key", key.c_str());

        perf_q = xQueueCreate(1, sizeof(int)); // to signal to http executor to start going

        transfer_request_queue = xQueueCreate(2, sizeof(Transfer)); // to request a OTA download
        http_control_queue = xQueueCreate(2, sizeof(control_message));
        http_data_buf = xStreamBufferCreate(4096, 3584); // transfers real data from network to its destination
        xTaskCreate(thread_fn, "http_loader", 4096, client, 0, &http_thread);

        xTaskCreate(http_performer, "http_performer", 4096, NULL, 0, &performer_thread);
    }

} // namespace HTTPManager