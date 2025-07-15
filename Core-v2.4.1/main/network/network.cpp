#include "network.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "storage.hpp"
#include <string.h>

#include "io/IO.hpp"

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "io/LEDControl.hpp"

#include <chrono>
#include <thread>

static const char* TAG = "network";

static TaskHandle_t network_task;
static QueueHandle_t network_event_queue;

static SemaphoreHandle_t is_online_mutex;
static bool is_online_value = false;
static int wifi_retry_count = 0;

static bool waiting_for_initial_connect = true;
static int wsacs_successive_failures = 0;

void set_is_networked(bool new_online) {
    if (xSemaphoreTake(is_online_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        is_online_value = new_online;
        xSemaphoreGive(is_online_mutex);
    }
}

bool Network::is_online() {
    if (is_online_mutex == NULL) {
        return false;
    }
    if (xSemaphoreTake(is_online_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        bool is_o = is_online_value;
        xSemaphoreGive(is_online_mutex);
        return is_o && WSACS::is_connected();
    }
    return false;
}

static void on_wifi_event(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        ESP_LOGI(TAG, "Wifi Started: Connecting....");
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_count < 5) {
            ESP_LOGW(TAG, "Wifi Reconnecting....");
            esp_wifi_connect();
            wifi_retry_count++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            ESP_LOGE(TAG, "WiFi Failed....");
            set_is_networked(false);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip: " IPSTR, IP2STR(&event->ip_info.ip));
        set_is_networked(true);

        Network::send_internal_event({
            .type = Network::InternalEventType::NetifUp,
            .netif_up_ip = event->ip_info.ip,
        });

        wifi_retry_count = 0;
    }
}

void wifi_init_sta(void) {
    WifiSSID ssid = Storage::get_network_ssid();
    WifiPassword pass = Storage::get_network_password();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, &instance_any_id));
    ESP_ERROR_CHECK(
        esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, ssid.data(), sizeof(ssid));
    memcpy(wifi_config.sta.password, pass.data(), sizeof(pass));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi thinking.");
}

const char* reset_reason_to_str(esp_reset_reason_t res) {
    switch (res) {
        case ESP_RST_UNKNOWN:
            return "UNKNOWN";
        case ESP_RST_POWERON:
            return "POWERON";
        case ESP_RST_EXT:
            return "EXT";
        case ESP_RST_SW:
            return "SW";
        case ESP_RST_PANIC:
            return "PANIC";
        case ESP_RST_INT_WDT:
            return "INT_WDT";
        case ESP_RST_TASK_WDT:
            return "TASK_WDT";
        case ESP_RST_WDT:
            return "WDT";
        case ESP_RST_DEEPSLEEP:
            return "DEEPSLEEP";
        case ESP_RST_BROWNOUT:
            return "BROWNOUT";
        case ESP_RST_SDIO:
            return "SDIO";
        case ESP_RST_USB:
            return "USB";
        case ESP_RST_JTAG:
            return "JTAG";
        case ESP_RST_EFUSE:
            return "EFUSE";
        case ESP_RST_PWR_GLITCH:
            return "PWR_GLITCH";
        case ESP_RST_CPU_LOCKUP:
            return "CPU_LOCKUP";
    }
    return "UNKNOWN";
}


bool already_sent_reset_reason = false;
void consider_reset_reason() {
    if (already_sent_reset_reason){
        return;
    }
    already_sent_reset_reason = true;
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset cause: %s", reset_reason_to_str(reason));
    if (reason == ESP_RST_PANIC) {
        ESP_LOGE(TAG, "Upload coredump");
    }
    std::string str = "Reset reason ";
    str += reset_reason_to_str(reason);
    char* msg = new char[str.size() + 1];
    strcpy(msg, str.c_str());
    Network::send_event({.type = NetworkEventType::Message, .message = msg});
}

static const AuthRequest invalid_auth = {.requester = CardTagID{}, .to_state = IOState::RESTART};
static AuthRequest outstanding_auth = invalid_auth;
static TimerHandle_t wsacs_timeout_timer_handle;
static TimerHandle_t watchdog_timer_handle;

namespace Network {
    // TODO make this think about things harder and do stuff if we're falling offline
    void network_watchdog_feed() {
        xTimerReset(watchdog_timer_handle, pdMS_TO_TICKS(100));
    }
    void network_watchdog_expiry_fn(TimerHandle_t) {
        Network::send_internal_event({.type = InternalEventType::ServerDown});
    }

    void handle_external_event(NetworkEvent event) {
        switch (event.type) {
            case NetworkEventType::AuthRequest:
                outstanding_auth = event.auth_request;
                // do fancier things in case this fails (ask storage)
                xTimerStart(wsacs_timeout_timer_handle, pdMS_TO_TICKS(100));
                WSACS::send_event(WSACS::Event::auth_req(event.auth_request));
                break;

            case NetworkEventType::Message:
                WSACS::send_event(WSACS::Event::message(event.message));
                break;

            case NetworkEventType::PleaseRestart:
                ESP_LOGE(TAG, "going kaboom");
                vTaskDelay(pdMS_TO_TICKS(1000));
                esp_restart();
                break;

            case NetworkEventType::StateChange:
                std::string str = "Changed state from ";
                str += io_state_to_string(event.state_change.from);
                str += " -> ";
                str += io_state_to_string(event.state_change.to);
                char* msg = new char[str.size() + 1];
                strcpy(msg, str.c_str());
                WSACS::send_event(WSACS::Event::message(msg));
                break;
        }
    }

    void network_thread_fn(void* p) {
        wifi_init_sta();

        while (true) {
            Network::InternalEvent event{InternalEventType::ExternalEvent}; // always overwritten
            if (xQueueReceive(network_event_queue, (void*)&event, portMAX_DELAY) == pdFALSE) {
                continue;
            }

            if (event.type == InternalEventType::ExternalEvent) {
                handle_external_event(event.external_event);
            } else if (event.type == InternalEventType::ServerUp) {
                consider_reset_reason(); // upload it
                xTimerStart(watchdog_timer_handle, pdMS_TO_TICKS(100));

                wsacs_successive_failures = 0;
                if (waiting_for_initial_connect) {
                    waiting_for_initial_connect = false;
                }
            } else if (event.type == InternalEventType::ServerDown) {
                wsacs_successive_failures += 1;
                // Quick falloff, might be able to recover
                if (wsacs_successive_failures < 5) {
                    WSACS::send_event({WSACS::EventType::TryConnect});
                }
            } else if (event.type == InternalEventType::NetifUp) {
                ESP_LOGD(TAG, "Wifi up, tell wsacs to connect");
                WSACS::send_event({WSACS::EventType::TryConnect});
            } else if (event.type == InternalEventType::ServerSetState) {
                IO::send_event({
                    .type = IOEventType::NETWORK_COMMAND,
                    .network_command =
                        {
                            .type = NetworkCommandEventType::COMMAND_STATE,
                            .commanded_state = event.server_set_state,
                            .requested = false,
                            .for_user = CardTagID{},
                        },
                });
            } else if (event.type == InternalEventType::WSACSTimedOut) {
                if (waiting_for_initial_connect) {
                    waiting_for_initial_connect = false;
                    // TODO load from flash
                    IOState last_state = IOState::IDLE;
                    IO::send_event({.type = IOEventType::NETWORK_COMMAND,
                                    .network_command = {
                                        .type = NetworkCommandEventType::COMMAND_STATE,
                                        .commanded_state = last_state,
                                        .requested = false,
                                    }});
                } else {
                    network_watchdog_expiry_fn(watchdog_timer_handle); // equiv to failing watchdog
                    IO::send_event({
                        .type = IOEventType::NETWORK_COMMAND,
                        .network_command = {.type = NetworkCommandEventType::DENY,
                                            .commanded_state = event.wsacs_auth_response.to_state,
                                            .requested = true,
                                            .for_user = event.wsacs_auth_response.user},
                    });
                }
            } else if (event.type == InternalEventType::WSACSAuthResponse) {
                xTimerStop(wsacs_timeout_timer_handle, pdMS_TO_TICKS(100));
                if (event.wsacs_auth_response.verified) {
                    IO::send_event({
                        .type = IOEventType::NETWORK_COMMAND,
                        .network_command =
                            {
                                .type = NetworkCommandEventType::COMMAND_STATE,
                                .commanded_state = event.wsacs_auth_response.to_state,
                                .requested = true,
                                .for_user = event.wsacs_auth_response.user,
                            },
                    });
                } else {
                    IO::send_event({
                        .type = IOEventType::NETWORK_COMMAND,
                        .network_command =
                            {
                                .type = NetworkCommandEventType::DENY,
                                .commanded_state = event.wsacs_auth_response.to_state,
                                .requested = true,
                                .for_user = event.wsacs_auth_response.user,
                            },
                    });
                }
            }
        }
        return;
    }

    int send_internal_event(InternalEvent ev) {
        xQueueSend(network_event_queue, &ev, pdMS_TO_TICKS(100));
        return 0;
    }

    int send_event(NetworkEvent ev) {
        return send_internal_event(InternalEvent{
            .type = InternalEventType::ExternalEvent,
            .external_event = ev,
        });
    }

    int init() {

        esp_log_level_set("esp-tls",
                          ESP_LOG_INFO); // enable INFO logs from DHCP client
        esp_log_level_set("transport_ws",
                          ESP_LOG_INFO); // enable INFO logs from DHCP client

        network_event_queue = xQueueCreate(5, sizeof(Network::InternalEvent));

        WSACS::init();
        is_online_mutex = xSemaphoreCreateMutex();

        // Timeout when we ask the server things
        wsacs_timeout_timer_handle =
            xTimerCreate("wsacs_timeout", pdMS_TO_TICKS(5 * 1000), pdFALSE, NULL,
                         [](TimerHandle_t) { send_internal_event({.type = InternalEventType::WSACSTimedOut}); });
        xTimerStart(wsacs_timeout_timer_handle, pdMS_TO_TICKS(100));

        // timeout for ping ponging
        watchdog_timer_handle =
            xTimerCreate("watchdog_timeout", pdMS_TO_TICKS(30 * 1000), pdFALSE, NULL, network_watchdog_expiry_fn);

        xTaskCreate(network_thread_fn, "network", CONFIG_NETWORK_TASK_STACK_SIZE, nullptr, 0, &network_task);

        esp_reset_reason_t reason = esp_reset_reason();
        ESP_LOGI(TAG, "Reset cause: %s", reset_reason_to_str(reason));

        return 0;
    }

} // namespace Network
