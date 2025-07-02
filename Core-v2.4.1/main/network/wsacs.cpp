#include "wsacs.hpp"

#include "esp_log.h"
#include "esp_websocket_client.h"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include "network/network.hpp"
#include "cJSON.h"

static const char* TAG = "wsacs";

QueueHandle_t wsacs_queue;
TaskHandle_t wsacs_thread;


bool opening_sent = false;
uint64_t seqnum = 0;
esp_websocket_client_handle_t ws_handle = NULL;
esp_websocket_client_config_t cfg{0};

uint64_t get_next_seqnum(){
    uint64_t i = seqnum;
    seqnum++;
    return i;
}

void handle_incoming_ws_text(const char *data, size_t len){

}

void send_opening_message(){
    if (ws_handle == NULL){
        ESP_LOGE(TAG, "Programming error");
        return;
    }
    cJSON *msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "SerialNumber", "1234");
    cJSON_AddStringToObject(msg, "Key", "abcd");
    cJSON_AddStringToObject(msg, "HWType", "Core");
    cJSON_AddStringToObject(msg, "HWVersion", "2.4.1");
    cJSON_AddStringToObject(msg, "FWVersion", "testing");
    cJSON_AddNumberToObject(msg, "Seq", (double)get_next_seqnum());

    char * text = cJSON_Print(msg);
    ESP_LOGI(TAG, "Sending message %s", text);
    free((void*)text);
    cJSON_free(msg);

}

static void websocket_event_handler(void* handler_args, esp_event_base_t base,
                                    int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_BEGIN:
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        send_opening_message();
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");

        if (data->error_handle.esp_ws_handshake_status_code != 0) {
            ESP_LOGE(TAG, "HTTP STATUS CODE: %d",
                     data->error_handle.esp_ws_handshake_status_code);
        }
        if (data->error_handle.error_type ==
            WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "TCP ERR");
            ESP_LOGE(TAG, "reported from esp-tls: %s", esp_err_to_name(data->error_handle.esp_tls_last_esp_err));
            ESP_LOGE(TAG, "reported from tls stack: %d",
                                 data->error_handle.esp_tls_stack_err);
            ESP_LOGE(TAG, "captured as transport's socket errno: %d",
                                 data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DATA");
        if (data->op_code == 0x2) { // Opcode 0x2 indicates binary data
            ESP_LOGW(TAG, "Received binary data, DROPPING");
        } else {
            handle_incoming_ws_text(data->data_ptr, data->data_len);
        }
        break;
    case WEBSOCKET_EVENT_ERROR:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
        if (data->error_handle.esp_ws_handshake_status_code != 0) {
            ESP_LOGE(TAG, "HTTP STATUS CODE: %d",
                     data->error_handle.esp_ws_handshake_status_code);
        }
        if (data->error_handle.error_type ==
            WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "reported from esp-tls: %s", esp_err_to_name(data->error_handle.esp_tls_last_esp_err));
            ESP_LOGE(TAG, "reported from tls stack: %d",
                                 data->error_handle.esp_tls_stack_err);
            ESP_LOGE(TAG, "captured as transport's socket errno: %d",
                                 data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_FINISH:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
        break;
    }
}

void connect_to_server() {
    if (ws_handle != NULL) {
        ESP_LOGW(TAG, "Destroying old ws client");
        esp_websocket_client_destroy(ws_handle);
        ws_handle = NULL;
        opening_sent = false;
        seqnum = 0;
    }
    cfg.uri                  = "ws://calcarea.student.rit.edu";
    cfg.port                 = 80;
    cfg.network_timeout_ms   = 10000;
    cfg.reconnect_timeout_ms = 1000;

    ws_handle = esp_websocket_client_init(&cfg);
    if (ws_handle == NULL) {
        ESP_LOGE(TAG, "Couldn't init client");
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    esp_websocket_register_events(ws_handle, WEBSOCKET_EVENT_ANY,
                                  websocket_event_handler, (void*)ws_handle);
    esp_err_t err = esp_websocket_client_start(ws_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Couldnt open ws: %s", esp_err_to_name(err));
        return;
    }
    vTaskDelay(pdMS_TO_TICKS(100));
    if (esp_websocket_client_is_connected(ws_handle)) {
        ESP_LOGI(TAG, "Connected to ws");
    } else {
        ESP_LOGE(TAG, "Couldn't connect");
        return;
    }

    err = esp_websocket_client_send_text(ws_handle, "hello im a shlug ", 16,
                                         pdMS_TO_TICKS(1000));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Couldnt send text on ws: %s", esp_err_to_name(err));
        return;
    }
}

void wsacs_thread_fn(void*) {
    WSACS::Event event;
    while (true) {
        if (xQueueReceive(wsacs_queue, (void*)&event, portMAX_DELAY) ==
            pdFALSE) {
            continue;
        }
        if (event.type == WSACS::EventType::WifiUp) {
            connect_to_server();
        }
    }
}

namespace WSACS {

    int init() {
        esp_log_level_set("TRANS_TCP", ESP_LOG_DEBUG);

        wsacs_queue = xQueueCreate(5, sizeof(WSACS::Event));
        if (wsacs_queue == NULL) {
            ESP_LOGE(TAG, "Fail and die");
            // todo crash
            return -1;
        }
        xTaskCreate(wsacs_thread_fn, "wsacs", 2000, NULL, 0, &wsacs_thread);
        if (wsacs_thread == NULL) {
            ESP_LOGE(TAG, "Fail and die task edition");
            // todo crash
            return -1;
        }
        return 0;
    }

    esp_err_t send_message(WSACS::Event event) {
        xQueueSend(wsacs_queue, &event, pdMS_TO_TICKS(100));
        return ESP_OK; // todo real code
    }

} // namespace WSACS