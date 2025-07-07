#include "esp_log.h"
#include "esp_netif_types.h"
#include "netdb.h"
#include "ping/ping_sock.h"

#define TAG "ping"

static void test_on_ping_success(esp_ping_handle_t hdl, void* args) {
    // optionally, get callback arguments
    // const char* str = (const char*) args;
    // printf("%s\r\n", str); // "foo"
    uint8_t ttl;
    uint16_t seqno;
    uint32_t elapsed_time, recv_len;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                         sizeof(target_addr));
    esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
    esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time,
                         sizeof(elapsed_time));
    ESP_LOGI(TAG, "%ld bytes from %s icmp_seq=%d ttl=%d time=%ld ms\n",
             recv_len, inet_ntoa(target_addr.u_addr.ip4), seqno, ttl,
             elapsed_time);
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void* args) {
    uint16_t seqno;
    ip_addr_t target_addr;
    esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
    esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                         sizeof(target_addr));
    ESP_LOGI(TAG, "From %s icmp_seq=%d timeout\n",
             inet_ntoa(target_addr.u_addr.ip4), seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void* args) {
    uint32_t transmitted;
    uint32_t received;
    uint32_t total_time_ms;

    esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted,
                         sizeof(transmitted));
    esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
    esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms,
                         sizeof(total_time_ms));
    ESP_LOGI(TAG, "%ld packets transmitted, %ld received, time %ldms\n",
             transmitted, received, total_time_ms);
}

void initialize_ping() {
    /* convert URL to IP address */
    ip_addr_t target_addr;
    struct addrinfo hint;
    struct addrinfo* res = NULL;
    memset(&hint, 0, sizeof(hint));
    memset(&target_addr, 0, sizeof(target_addr));
    getaddrinfo("www.espressif.com", NULL, &hint, &res);
    struct in_addr addr4 = ((struct sockaddr_in*)(res->ai_addr))->sin_addr;
    inet_addr_to_ip4addr(ip_2_ip4(&target_addr), &addr4);
    ESP_LOGI(TAG, "Pinging " IPSTR, (int)(addr4.s_addr & 0xff),
             (int)((addr4.s_addr >> 8) & 0xff),
             (int)((addr4.s_addr >> 16) & 0xff), (int)(addr4.s_addr >> 24));
    freeaddrinfo(res);

    esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
    ping_config.target_addr       = target_addr; // target IP address
    ping_config.count = ESP_PING_COUNT_INFINITE; // ping in infinite mode,
                                                 // esp_ping_stop can stop it

    /* set callback functions */
    esp_ping_callbacks_t cbs;
    cbs.on_ping_success = test_on_ping_success;
    cbs.on_ping_timeout = test_on_ping_timeout;
    cbs.on_ping_end     = test_on_ping_end;
    cbs.cb_args         = (void*)"foo"; // arguments that feeds to all callback
                                        // functions, can be NULL
    // cbs.cb_args = eth_event_group;

    esp_ping_handle_t ping;
    esp_err_t err = esp_ping_new_session(&ping_config, &cbs, &ping);
    if (err != ESP_OK){
        ESP_LOGI(TAG, "Failed to init ping: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "starting ping");
    }
}