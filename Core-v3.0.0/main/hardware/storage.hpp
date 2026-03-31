#include "common/types.hpp"
#include <cstdint>
#include <string>

namespace Storage {

    int init();

    std::string get_key();
    WifiSSID get_network_ssid();
    WifiPassword get_network_password();
    std::string get_server();
    const char* get_server_certs();
    uint8_t get_max_temp();


    bool set_key(std::string key);
    bool set_network_ssid(WifiSSID ssid);
    bool set_network_password(WifiPassword password);
    bool set_server(std::string server);
    bool set_max_temp(uint8_t max_temp);

    int check_perms(const CardTagID& uid, bool& can_change_state, bool& can_access);

} // namespace Storage