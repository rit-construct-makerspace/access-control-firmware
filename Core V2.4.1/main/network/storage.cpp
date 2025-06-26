#include "storage.hpp"

namespace Storage {

std::string get_network_ssid() {
  std::string returnValue = "RIT-WiFi";
  return returnValue;
}

std::string get_network_password() {
  std::string returnValue = "";
  return returnValue;
}

std::string get_server() {
  std::string returnValue = "calcarea.student.rit.edu";
  return returnValue;
}

std::string get_server_certs() {
  std::string returnValue = "";
  return returnValue;
}

std::string get_serial_number() {
  std::string returnValue = "1234567890";
  return returnValue;
}

std::string get_key() {
  std::string returnValue = "ABCDEFGHIJ";
  return returnValue;
}

bool set_network_ssid(std::string ssid) { return false; }

bool set_network_password(std::string password) { return false; }

bool set_server(std::string server) { return false; }

} // namespace Storage