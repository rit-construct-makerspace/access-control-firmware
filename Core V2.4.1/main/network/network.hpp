#include "common/types.hpp"

namespace Network {
int init();

void send_state_transition();

bool is_network_online();
} // namespace Network
void debug_log(const LogMessage &msg);