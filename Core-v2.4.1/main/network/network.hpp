#pragma once
#include "common/types.hpp"

namespace Network {
int init();

void report_state_transition(IOState from, IOState to);
bool is_online();

} // namespace Network
