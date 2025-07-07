#include "wsacs.hpp"

#include "cJSON.h"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "io/IO.hpp"
#include "network/network.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>

static const char* TAG = "wsacs";

QueueHandle_t wsacs_queue;
TaskHandle_t wsacs_thread;

uint64_t seqnum                         = 0;
TimerHandle_t keep_alive_timer          = NULL;
esp_websocket_client_handle_t ws_handle = NULL;
esp_websocket_client_config_t cfg{};

uint64_t get_next_seqnum() {
    uint64_t i = seqnum;
    seqnum++;
    return i;
}

bool parse_iostate(const char* str, IOState& out) {
    constexpr size_t max_size = 50;
    int len                   = strnlen(str, max_size);
    if (len == max_size) {
        // return if bigger or not null terminated
        return false;
    }
    if (0 == strcasecmp(str, "idle")) {
        out = IOState::IDLE;
        return true;
    } else if (0 == strcasecmp(str, "unlocked")) {
        out = IOState::UNLOCKED;
        return true;
    } else if (0 == strcasecmp(str, "alwayson")) {
        out = IOState::ALWAYS_ON;
        return true;
    } else if (0 == strcasecmp(str, "lockout")) {
        out = IOState::LOCKOUT;
        return true;
    } else if (0 == strcasecmp(str, "nextcard")) {
        out = IOState::NEXT_CARD;
        return true;
    } else if (0 == strcasecmp(str, "startup")) {
        out = IOState::STARTUP;
        return true;
    } else if (0 == strcasecmp(str, "welcoming")) {
        out = IOState::WELCOMING;
        return true;
    } else if (0 == strcasecmp(str, "welcomed")) {
        out = IOState::WELCOMED;
        return true;
    } else if (0 == strcasecmp(str, "alwaysonwaiting")) {
        out = IOState::ALWAYS_ON_WAITING;
        return true;
    } else if (0 == strcasecmp(str, "alwaysonwaiting")) {
        out = IOState::ALWAYS_ON_WAITING;
        return true;
    } else if (0 == strcasecmp(str, "idlewaiting")) {
        out = IOState::IDLE_WAITING;
        return true;
    } else if (0 == strcasecmp(str, "awaitauth")) {
        out = IOState::AWAIT_AUTH;
        return true;
    } else if (0 == strcasecmp(str, "denied")) {
        out = IOState::DENIED;
        return true;
    } else if (0 == strcasecmp(str, "fault")) {
        out = IOState::FAULT;
        return true;
    } else if (0 == strcasecmp(str, "restart")) {
        out = IOState::RESTART;
        return true;
    }
    return false;
}

void handle_server_state_change(const char* state) {
    IOState iostate = IOState::IDLE;
    if (!parse_iostate(state, iostate)) {
        ESP_LOGE(TAG, "Not setting state to unknown state: %s", state);
        return;
    }
    ESP_LOGI(TAG, "send internal event: %s", io_state_to_string(iostate));
    Network::send_internal_event({
        .type             = Network::InternalEventType::ServerSetState,
        .server_set_state = iostate,
    });
}

IOState outstanding_tostate = IOState::IDLE; // todo, should be sent by the server

void handle_auth_response(const char *auth, int verified){
    std::optional<CardTagID> requester = CardTagID::from_string(auth);
    if (!requester.has_value()){
        ESP_LOGE(TAG, "Can't auth bc bad UID: %.14s", auth);
        return;
    }
    ESP_LOGI(TAG, "Handling auth response: %s - %d", auth, verified);

    Network::send_internal_event(Network::InternalEvent{
        .type = Network::InternalEventType::WSACSAuthResponse,
        .wsacs_auth_response = {   
            .user = requester.value(),
            .to_state = outstanding_tostate,
            .verified = (bool)verified,
        },
    });
}


void handle_incoming_ws_text(const char* data, size_t len) {
    if (len == 0) {
        ESP_LOGE(TAG, "ws message with 0 length???");
        return;
    }
    ESP_LOGI(TAG, "Received msg size %d: %.*s", len, len, data);

    cJSON* obj = cJSON_ParseWithLength(data, len);
    if (obj == NULL) {
        ESP_LOGE(TAG, "Failed to parse json: %.*s", len, data);
        return;
    }
    if (cJSON_HasObjectItem(obj, "State")) {
        ESP_LOGI(TAG, "handle state change: %s",
                 cJSON_GetStringValue(cJSON_GetObjectItem(obj, "State")));
        handle_server_state_change(
            cJSON_GetStringValue(cJSON_GetObjectItem(obj, "State")));
    }
    if (cJSON_HasObjectItem(obj, "Auth") && cJSON_HasObjectItem(obj, "Verified")) {
        const char * auth = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "Auth"));
        int verified = cJSON_GetNumberValue(cJSON_GetObjectItem(obj, "Verified"));        
        handle_auth_response(auth, verified);
    }


    cJSON_free(obj);
}

// will add sequence number and to string it (ONLY CALL ON WEBSOCKET THREAD)
void send_cjson(cJSON* obj) {
    cJSON_AddNumberToObject(obj, "Seq", (double)get_next_seqnum());

    char* text = cJSON_Print(obj);
    size_t len = strnlen(text, 1000);
    ESP_LOGI(TAG, "Sending message %s", text);

    int err = esp_websocket_client_send_text(ws_handle, text, len,
                                             pdMS_TO_TICKS(100));
    if (err != len) {
        ESP_LOGE(TAG, "Failed to send WS message: %d", err);
    }
    free((void*)text);
}

void send_keep_alive_message() {
    if (ws_handle == NULL) {
        ESP_LOGE(TAG, "Programming error");
        return;
    }
    IOState state = IOState::IDLE;
    if (!IO::get_state(state)) {
        ESP_LOGE(TAG, "Couldn't get state for status message");
        return;
    }
    int temp = 33;

    cJSON* msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "State", io_state_to_string(state));
    cJSON_AddNumberToObject(msg, "Temp", (double)temp);

    send_cjson(msg);
    cJSON_free(msg);
}

void send_opening_message() {
    if (ws_handle == NULL) {
        ESP_LOGE(TAG, "Programming error");
        return;
    }
    cJSON* msg = cJSON_CreateObject();
    cJSON_AddStringToObject(msg, "SerialNumber", "1234");
    cJSON_AddStringToObject(msg, "Key", "01bec4377c4906fb5264841a42532883");
    cJSON_AddStringToObject(msg, "HWType", "Core");
    cJSON_AddStringToObject(msg, "HWVersion", "2.4.1");
    cJSON_AddStringToObject(msg, "FWVersion", "testing");
    cJSON* req_arr = cJSON_AddArrayToObject(msg, "Request");
    cJSON* req0    = cJSON_CreateString("State");
    cJSON* req1    = cJSON_CreateString("Time");
    cJSON_AddItemToArray(req_arr, req0);
    cJSON_AddItemToArray(req_arr, req1);
    cJSON_AddStringToObject(msg, "FWVersion", "testing");

    send_cjson(msg);
    cJSON_free(msg);
}
void send_auth_request(AuthRequest request) {
    if (ws_handle == NULL) {
        ESP_LOGE(TAG, "Programming error");
        return;
    }
    outstanding_tostate = request.to_state;
    cJSON* msg      = cJSON_CreateObject();
    std::string uid = request.requester.to_string();
    cJSON_AddStringToObject(msg, "Auth", uid.c_str());
    cJSON_AddStringToObject(msg, "AuthTo",
                            io_state_to_string(request.to_state));

    send_cjson(msg);

    cJSON_free(msg);
}

static void websocket_event_handler(void* handler_args, esp_event_base_t base,
                                    int32_t event_id, void* event_data) {
    esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
    switch (event_id) {
    case WEBSOCKET_EVENT_BEGIN:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_BEGIN");
        break;
    case WEBSOCKET_EVENT_CONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_CONNECTED");
        WSACS::send_message(
            WSACS::Event{.type = WSACS::EventType::ServerConnect, ._ = {0}});
        break;
    case WEBSOCKET_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "WEBSOCKET_EVENT_DISCONNECTED");
        WSACS::send_message(
            WSACS::Event{.type = WSACS::EventType::ServerDisconnect});

        if (data->error_handle.esp_ws_handshake_status_code != 0) {
            ESP_LOGE(TAG, "HTTP STATUS CODE: %d",
                     data->error_handle.esp_ws_handshake_status_code);
        }
        if (data->error_handle.error_type ==
            WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
            ESP_LOGE(TAG, "TCP ERR");
            ESP_LOGE(TAG, "reported from esp-tls: %s",
                     esp_err_to_name(data->error_handle.esp_tls_last_esp_err));
            ESP_LOGE(TAG, "reported from tls stack: %d",
                     data->error_handle.esp_tls_stack_err);
            ESP_LOGE(TAG, "captured as transport's socket errno: %d",
                     data->error_handle.esp_transport_sock_errno);
        }
        break;
    case WEBSOCKET_EVENT_DATA:
        // network watchdog feed
        if (data->op_code == 0x1) { // Opcode 0x1 indicates text data
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
            ESP_LOGE(TAG, "reported from esp-tls: %s",
                     esp_err_to_name(data->error_handle.esp_tls_last_esp_err));
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
        seqnum    = 0;
        xTimerStop(keep_alive_timer, pdMS_TO_TICKS(100));
    }
    cfg.uri                  = "ws://calcarea.student.rit.edu/api/ws";
    cfg.port                 = 3000;
    cfg.network_timeout_ms   = 10000;
    cfg.reconnect_timeout_ms = 1000;

    ws_handle = esp_websocket_client_init(&cfg);
    if (ws_handle == NULL) {
        ESP_LOGE(TAG, "Couldn't init client");
        return;
    }
    esp_websocket_register_events(ws_handle, WEBSOCKET_EVENT_ANY,
                                  websocket_event_handler, (void*)ws_handle);
    esp_err_t err = esp_websocket_client_start(ws_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Couldnt open ws: %s", esp_err_to_name(err));
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
        if (event.type == WSACS::EventType::TryConnect) {
            connect_to_server();
        } else if (event.type == WSACS::EventType::ServerConnect) {
            ESP_LOGI(TAG, "Sending openning");
            seqnum = 0;
            send_opening_message();
            if (xTimerStart(keep_alive_timer, pdMS_TO_TICKS(100)) == pdFAIL) {
                ESP_LOGE(TAG, "Cpi;dnt srtart timer");
            }
        } else if (event.type == WSACS::EventType::ServerConnect) {
        } else if (event.type == WSACS::EventType::KeepAliveTimer) {
            send_keep_alive_message();
        } else if (event.type == WSACS::EventType::AuthRequest) {
            send_auth_request(event.auth_request);
        } else {

            ESP_LOGW(TAG, "Not handling message with type %d", (int)event.type);
        }
    }
}

namespace WSACS {

    int init() {
        esp_log_level_set("TRANS_TCP", ESP_LOG_DEBUG);

        wsacs_queue = xQueueCreate(5, sizeof(Event));
        if (wsacs_queue == NULL) {
            ESP_LOGE(TAG, "Fail and die");
            // todo crash
            return -1;
        }
        keep_alive_timer = xTimerCreate(
            "wsacs keepalive", pdMS_TO_TICKS(10 * 1000), true, NULL,
            [](TimerHandle_t) {
                WSACS::send_message({.type = WSACS::EventType::KeepAliveTimer});
            });

        if (keep_alive_timer == NULL) {
            ESP_LOGE(TAG, "Fail and die timer edition");
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

    esp_err_t send_message(Event event) {
        xQueueSend(wsacs_queue, &event, pdMS_TO_TICKS(100));
        return ESP_OK; // todo real code
    }

} // namespace WSACS