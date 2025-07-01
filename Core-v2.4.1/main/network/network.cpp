#include "network.hpp"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_wifi.h"
#include "sdkconfig.h"
#include "storage.hpp"
#include <string.h>

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>

#include "io/LEDControl.hpp"

#include <chrono>
#include <thread>

static const char* TAG = "network";

static TaskHandle_t network_task;


/* The event group allows multiple bits for each event, but we only care about
 * two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
/* FreeRTOS event group to signal when we are connected*/

static EventGroupHandle_t s_wifi_event_group;
static int s_retry_num = 0;

static void on_wifi_event(void* arg, esp_event_base_t event_base,
                          int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
        set_led_state(LEDDisplayState::IDLE);
    } else if (event_base == WIFI_EVENT &&
               event_id == WIFI_EVENT_STA_DISCONNECTED) {
        set_led_state(LEDDisplayState::LOCKOUT);
        if (s_retry_num < 5) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*)event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        set_led_state(LEDDisplayState::ALWAYS_ON);
        // submit_ip(event->ip_info.ip);

        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(void) {
    WifiSSID ssid     = Storage::get_network_ssid();
    WifiPassword pass = Storage::get_network_password();

    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &on_wifi_event, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &on_wifi_event, NULL, &instance_got_ip));

    wifi_config_t wifi_config = {};
    memcpy(wifi_config.sta.ssid, ssid.data(), sizeof(ssid));
    memcpy(wifi_config.sta.password, pass.data(), sizeof(pass));
    wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;
    wifi_config.sta.sae_pwe_h2e        = WPA3_SAE_PWE_HUNT_AND_PECK;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finished.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT)
     * or connection failed for the maximum number of re-tries (WIFI_FAIL_BIT).
     * The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we
     * can test which event actually happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", ssid.data(),
                 pass.data());
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", ssid.data(),
                 pass.data());
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
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

void consider_reset() {
    esp_reset_reason_t reason = esp_reset_reason();
    ESP_LOGI(TAG, "Reset cause: %s", reset_reason_to_str(reason));
    if (reason == ESP_RST_PANIC){
        ESP_LOGE(TAG, "Upload coredump");
    }
}

void network_thread_fn(void* p) {
    wifi_init_sta();
    int i = 0;
    while (true) {
        i++;
        ESP_LOGI(TAG, "Tick %d", i);
        vTaskDelay(pdMS_TO_TICKS(5000));
        // int i = *(int*)p;
        // ESP_LOGI(TAG, "I=%d, %p", i, p);
    }
    return;
}

namespace Network {
    int init() {
        consider_reset();

        ESP_LOGI(TAG, "Network Side Start");

        Storage::init(); // Storage must come before wifi as wifi depends on NVS
        xTaskCreate(network_thread_fn, "network", 8192, nullptr, 0,
                    &network_task);

        return 0;
    }

    void report_state_transition(IOState from, IOState to) {
        ESP_LOGI(TAG, "Transition %s -> %s", io_state_to_string(from),
                 io_state_to_string(to));
    }

    bool is_online() { return true; }
} // namespace Network

namespace bad {}
