#include "wsacs.hpp"
#include "io/BuzzerSounds.hpp"

#include "cJSON.h"
#include "common/hardware.hpp"
#include "esp_log.h"
#include "esp_websocket_client.h"
#include "io/Buzzer.hpp"
#include "io/IO.hpp"
#include "io/Temperature.hpp"
#include "network.hpp"
#include "network/network.hpp"
#include "storage.hpp"
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include "ota.hpp"

static const char* TAG = "wsacs";

esp_websocket_client_handle_t ws_handle = NULL;
esp_websocket_client_config_t cfg{};

SoundEffect::Effect network_song = {.length = 0, .notes = NULL};

namespace WSACS {
    static bool received_first_message = false;
    static bool already_got_state_from_server_for_this_boot = false;

    uint64_t seqnum = 0;
    uint64_t get_next_seqnum() {
        uint64_t i = seqnum;
        seqnum++;
        return i;
    }

    void handle_server_state_change(const char* state) {

        std::optional<IOState> iostate = parse_iostate(state);
        if (!iostate.has_value()) {
            ESP_LOGE(TAG, "Not setting state to unknown state: %.16s", state);
            return;
        }
        ESP_LOGI(TAG, "Received request to %s", io_state_to_string(iostate.value()));
        already_got_state_from_server_for_this_boot = true;

        IO::send_event({
            .type = IOEventType::NETWORK_COMMAND,
            .network_command =
                {
                    .type = NetworkCommandEventType::COMMAND_STATE,
                    .commanded_state = iostate.value(),
                    .requested = false,
                    .for_user = CardTagID{},
                },
        });
        if (iostate.value() == IOState::RESTART){
            Network::send_internal_event(Network::InternalEventType::PollRestart);
        }
    }

    IOState outstanding_tostate = IOState::IDLE; // todo, should be sent by the server
    void handle_auth_response(const char* auth, int verified, const char* error) {
        std::optional<CardTagID> requester = CardTagID::from_string(auth);
        if (!requester.has_value()) {
            ESP_LOGE(TAG, "Can't auth bc bad UID: %.14s", auth);
            verified = 0;
        }
        ESP_LOGI(TAG, "Handling auth response: %s - %d: %s - %s", auth, verified,
                 io_state_to_string(outstanding_tostate), error ? error : "no error");
        Network::mark_wsacs_request_complete();
        if (verified) {
            IO::send_event({
                .type = IOEventType::NETWORK_COMMAND,
                .network_command =
                    {
                        .type = NetworkCommandEventType::COMMAND_STATE,
                        .commanded_state = outstanding_tostate,
                        .requested = true,
                        .for_user = requester.value(),
                    },
            });
        } else {
            IO::send_event({
                .type = IOEventType::NETWORK_COMMAND,
                .network_command =
                    {
                        .type = NetworkCommandEventType::DENY,
                    },
            });
        }
    }

    void handle_song_request(cJSON* song) {
        if (!cJSON_HasObjectItem(song, "Notes")) {
            ESP_LOGW(TAG, "Song request had incorrect keys");
            return;
        }
        cJSON* notes_obj = cJSON_GetObjectItem(song, "Notes");
        if (!(notes_obj->type & cJSON_Array)) {
            ESP_LOGW(TAG, "Wrong type for song notes");
            return;
        }
        size_t length = cJSON_GetArraySize(notes_obj);
        SoundEffect::Note* notes = new SoundEffect::Note[length];

        cJSON* notedef = NULL;
        size_t i = 0;
        cJSON_ArrayForEach(notedef, notes_obj) {
            if (cJSON_GetArraySize(notedef) != 2) {
                ESP_LOGW(TAG, "Wrong number of items in array");
                delete[] notes;
                return;
            }
            cJSON* freq_obj = cJSON_GetArrayItem(notedef, 0);
            cJSON* len_obj = cJSON_GetArrayItem(notedef, 1);
            notes[i] = SoundEffect::Note{
                .frequency = (uint32_t)cJSON_GetNumberValue(freq_obj),
                .duration = (uint16_t)cJSON_GetNumberValue(len_obj),
            };
            i++;
        }

        network_song.length = length;
        if (network_song.notes != NULL) {
            delete[] network_song.notes;
        }
        network_song.notes = notes;
        ESP_LOGI(TAG, "Parsed song of %u notes", network_song.length);
    }

    void handle_incoming_ws_text(const char* data, size_t len) {
        if (!received_first_message){
            Network::send_internal_event(Network::InternalEventType::ServerAuthed);
            received_first_message = true;
        }
        if (len == 0) {
            ESP_LOGE(TAG, "ws message with 0 length. Protocol Error");
            return;
        }
        ESP_LOGD(TAG, "Received msg size %d: %.*s", len, len, data);

        cJSON* obj = cJSON_ParseWithLength(data, len);
        if (obj == NULL) {
            ESP_LOGE(TAG, "Failed to parse json: %.*s", len, data);
            return;
        }

        if (cJSON_HasObjectItem(obj, "State")) {
            cJSON* state_obj = cJSON_GetObjectItem(obj, "State");
            if (state_obj->type & cJSON_String) {
                handle_server_state_change(cJSON_GetStringValue(state_obj));
            }
        }

        if (cJSON_HasObjectItem(obj, "Auth")) {
            int verified = 0;
            if (cJSON_HasObjectItem(obj, "Verified")) {
                verified = cJSON_GetNumberValue(cJSON_GetObjectItem(obj, "Verified"));
            }
            const char* auth = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "Auth"));
            const char* error = cJSON_GetStringValue(cJSON_GetObjectItem(obj, "Error"));
            handle_auth_response(auth, verified, error);
        }

        if (cJSON_HasObjectItem(obj, "Identify")) {
            IO::send_event({
                .type = IOEventType::NETWORK_COMMAND,
                .network_command =
                    {
                        .type = NetworkCommandEventType::IDENTIFY,
                    },
            });
        }
        if (cJSON_HasObjectItem(obj, "Song")) {
            handle_song_request(cJSON_GetObjectItem(obj, "Song"));
        }
        if (cJSON_HasObjectItem(obj, "PlaySong")) {
            if (cJSON_IsTrue(cJSON_GetObjectItem(obj, "PlaySong"))) {
                Buzzer::send_effect(network_song);
            }
        }
        if (cJSON_HasObjectItem(obj, "OTATag")) {
            cJSON* ota_ver = cJSON_GetObjectItem(obj, "OTATag");
            if (ota_ver->type & cJSON_String) {
                Network::InternalEvent ie{.type = Network::InternalEventType::OtaUpdate, .ota_tag = {}};
                strncpy(ie.ota_tag.data(), cJSON_GetStringValue(ota_ver), sizeof(ie.ota_tag));
                Network::send_internal_event(ie);
            } else {
                ESP_LOGW(TAG, "Invalid type for OTATag tag: %d", ota_ver->type);
            }
        }

        cJSON_Delete(obj);
    }

    // will add sequence number and to string it (ONLY CALL ON THREAD THAT OWNS WEBSOCKET)
    esp_err_t send_cjson(cJSON* obj) {

        cJSON_AddNumberToObject(obj, "Seq", (double)get_next_seqnum());

        char* text = cJSON_Print(obj);
        size_t len = strnlen(text, 5000);
        ESP_LOGI(TAG, "Sending message %s", text);

        int err = esp_websocket_client_send_text(ws_handle, text, len, pdMS_TO_TICKS(100));
        if (err != len) {
            ESP_LOGE(TAG, "Failed to send WS message: %d", err);
            Network::send_internal_event(Network::InternalEventType::ServerDown);
        }
        cJSON_free((void*)text);
        return ESP_OK;
    }

    static IOState last_valid_state = IOState::STARTUP;
    bool is_serverable_state(IOState st) {
        switch (st) {
            case IOState::IDLE:
            case IOState::ALWAYS_ON:
            case IOState::FAULT:
            case IOState::LOCKOUT:
            case IOState::WELCOMING:
            case IOState::UNLOCKED:
            case IOState::STARTUP:
                return true;
            default:
                return false;
        }
    }
    void send_status_message() {
        cJSON* msg = NULL;
        if (ws_handle == NULL) {
            // try reconnecting
            ESP_LOGW(TAG, "Trying to keepalive on no connection? not doing anything");
            return;
        }

        IOState state = IOState::IDLE;
        if (!IO::get_state(state)) {
            ESP_LOGE(TAG, "Couldn't get state for status message");
            return;
        }
        if (is_serverable_state(state)) {
            last_valid_state = state;
        }
        float temp = 33;
        Temperature::get_temp(temp);

        msg = cJSON_CreateObject();

        cJSON_AddStringToObject(msg, "State", io_state_to_string(last_valid_state));
        cJSON_AddNumberToObject(msg, "Temp", (double)temp);
        if (OTA::next_app_version()!=""){
            cJSON_AddStringToObject(msg, "FEVer", OTA::next_app_version().c_str());
        }
        send_cjson(msg);

        cJSON_Delete(msg);
    }

    void send_opening_message() {
        if (ws_handle == NULL) {
            ESP_LOGE(TAG, "Programming error. No WS handle when sending opening message");
            return;
        }
        cJSON* msg = cJSON_CreateObject();
        cJSON_AddStringToObject(msg, "SerialNumber", Hardware::get_serial_number());
#ifdef DEV_SERVER
        cJSON_AddStringToObject(
            msg, "Key",
            "07edfd78f2a97d0d2c46c1cb4504fbe343a9bb6ec7f2a64b41d2c7d4f6fcca7f63f78220b70230e3f022e395fe0eb436");
#else
        cJSON_AddStringToObject(msg, "Key", Storage::get_key().c_str());

#endif
        cJSON_AddStringToObject(msg, "HWType", "Core");

        cJSON_AddStringToObject(msg, "HWVersion", Hardware::get_edition_string());
        cJSON_AddStringToObject(msg, "BEVer", OTA::running_app_version().c_str());
        cJSON_AddStringToObject(msg, "FEVer", OTA::next_app_version().c_str());

        cJSON* req_arr = cJSON_AddArrayToObject(msg, "Request");
        cJSON* req0 = cJSON_CreateString("Time");
        cJSON_AddItemToArray(req_arr, req0);
        if (!already_got_state_from_server_for_this_boot) {
            // Only need to ask on first boot, if we just dropped a connection for a sec
            // we shouldnt go back to idle or anything
            cJSON* req1 = cJSON_CreateString("State");
            cJSON_AddItemToArray(req_arr, req1);
            cJSON* req2 = cJSON_CreateString("OTATag");
            cJSON_AddItemToArray(req_arr, req2);
        }

        send_cjson(msg);
        cJSON_Delete(msg);
    }

    esp_err_t send_message(char* msg) {
        cJSON* obj = cJSON_CreateObject();
        cJSON_AddStringToObject(obj, "Message", msg);
        return send_cjson(obj);
    }

    void send_auth_request(const AuthRequest& request) {
        if (ws_handle == NULL) {
            ESP_LOGE(TAG, "Programming error");
            return;
        }
        outstanding_tostate = request.to_state;
        cJSON* msg = cJSON_CreateObject();
        std::string uid = request.requester.to_string();
        cJSON_AddStringToObject(msg, "Auth", uid.c_str());
        cJSON_AddStringToObject(msg, "AuthTo", io_state_to_string(request.to_state));

        send_cjson(msg);

        cJSON_Delete(msg);
    }

    static void websocket_event_handler(void* handler_args, esp_event_base_t base, int32_t event_id, void* event_data) {
        esp_websocket_event_data_t* data = (esp_websocket_event_data_t*)event_data;
        switch (event_id) {
            case WEBSOCKET_EVENT_BEGIN:
                break;
            case WEBSOCKET_EVENT_CONNECTED:
                Network::send_internal_event(Network::InternalEventType::ServerUp);
                break;
            case WEBSOCKET_EVENT_DISCONNECTED:
                if (data->error_handle.esp_ws_handshake_status_code != 0) {
                    ESP_LOGE(TAG, "HTTP STATUS CODE: %d", data->error_handle.esp_ws_handshake_status_code);
                }
                if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                    ESP_LOGE(TAG, "TCP ERR");
                    ESP_LOGE(TAG, "reported from esp-tls: %s",
                             esp_err_to_name(data->error_handle.esp_tls_last_esp_err));
                    ESP_LOGE(TAG, "reported from tls stack: %d", data->error_handle.esp_tls_stack_err);
                    ESP_LOGE(TAG, "captured as transport's socket errno: %d",
                             data->error_handle.esp_transport_sock_errno);
                }
                break;
            case WEBSOCKET_EVENT_DATA:
                Network::network_watchdog_feed();
                // TODO check close code
                if (data->op_code == 0x1) { // Opcode 0x1 indicates text data
                    handle_incoming_ws_text(data->data_ptr, data->data_len);
                } else if (data->op_code == 0x8) { // WS_TRANSPORT_OPCODES_CLOSE
                    ESP_LOGE(TAG, "Websocket closed");
                    Network::send_internal_event(Network::InternalEventType::ServerDown);
                }
                break;
            case WEBSOCKET_EVENT_ERROR:
                ESP_LOGI(TAG, "WEBSOCKET_EVENT_ERROR");
                if (data->error_handle.esp_ws_handshake_status_code != 0) {
                    ESP_LOGE(TAG, "HTTP STATUS CODE: %d", data->error_handle.esp_ws_handshake_status_code);
                }
                if (data->error_handle.error_type == WEBSOCKET_ERROR_TYPE_TCP_TRANSPORT) {
                    ESP_LOGE(TAG, "reported from esp-tls: %s",
                             esp_err_to_name(data->error_handle.esp_tls_last_esp_err));
                    ESP_LOGE(TAG, "reported from tls stack: %d", data->error_handle.esp_tls_stack_err);
                    ESP_LOGE(TAG, "captured as transport's socket errno: %d",
                             data->error_handle.esp_transport_sock_errno);
                }
                Network::send_internal_event(Network::InternalEventType::ServerDown);
                break;
            case WEBSOCKET_EVENT_FINISH:
                ESP_LOGI(TAG, "WEBSOCKET_EVENT_FINISH");
                break;
        }
    }

    esp_err_t try_connect() {
        seqnum = 0;
        received_first_message = false;
        if (esp_websocket_client_is_connected(ws_handle)) {
            // Already up
            return ESP_OK;
        }
        esp_websocket_client_stop(ws_handle);

        esp_err_t err = esp_websocket_client_start(ws_handle);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Couldnt open ws: %s", esp_err_to_name(err));
            return err;
        }
        return ESP_OK;
    }

    esp_err_t init() {
#ifdef DEV_SERVER
        std::string websocket_url = "ws://" DEV_SERVER "/api/ws";
        cfg.uri = websocket_url.c_str();
        cfg.port = 3000;
#else
        std::string server_url = Storage::get_server();
        std::string websocket_url = "wss://" + server_url + "/api/ws";
        cfg.uri = websocket_url.c_str();
        cfg.cert_pem = Storage::get_server_certs();
        cfg.cert_len = 0; // use strlen
#endif

        cfg.network_timeout_ms = 10000;
        cfg.reconnect_timeout_ms = 3000;

        ws_handle = esp_websocket_client_init(&cfg);
        if (ws_handle == NULL) {
            ESP_LOGE(TAG, "Couldn't init client");
            return ESP_ERR_INVALID_ARG;
        }
        esp_websocket_register_events(ws_handle, WEBSOCKET_EVENT_ANY, websocket_event_handler, (void*)ws_handle);

        return ESP_OK;
    }

} // namespace WSACS