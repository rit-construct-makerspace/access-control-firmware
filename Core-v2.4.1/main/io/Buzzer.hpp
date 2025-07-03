#pragma once

#include "BuzzerSounds.hpp"

namespace Buzzer {
    int init();
    bool send_effect(SoundEffect::Effect);
}