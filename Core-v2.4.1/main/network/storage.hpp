#include "common/types.hpp"
#include <cstdint>
#include <string>

namespace Hardware{
    int identify();
    const char *get_serial_number();
    HardwareEdition get_edition();
    const char *get_edition_string();
}

namespace Storage {

    int init();

    WifiSSID get_network_ssid();
    WifiPassword get_network_password();
    std::string get_server();
    std::string get_server_certs();

    bool set_key(std::string key);
    std::string get_key();

    bool set_network_ssid(WifiSSID ssid);
    bool set_network_password(WifiPassword password);
    bool set_server(std::string server);

    int check_perms(const CardTagID &uid, bool &can_change_state, bool &can_access);


} // namespace Storage