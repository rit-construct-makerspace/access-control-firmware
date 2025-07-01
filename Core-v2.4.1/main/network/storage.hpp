#include "common/types.hpp"
#include <cstdint>
#include <string>

namespace Storage {

    int init();

    WifiSSID get_network_ssid();
    WifiPassword get_network_password();
    std::string get_server();
    std::string get_server_certs();

    std::string get_serial_number();
    std::string get_key();

    bool set_network_ssid(std::string ssid);
    bool set_network_password(std::string password);
    bool set_server(std::string server);

} // namespace Storage