#pragma once

#include "common/types.hpp"

namespace CardReader {
    void init();
    bool get_card_tag(CardTagID& ret_tag);
    bool card_present();
    bool set_require_switches(bool require_state);
} // namespace CardReader