#pragma once
#include "common/types.hpp"

namespace IO {
    int init();
    bool get_state(IOState &send_state);
    bool send_event(IOEvent event); 
};