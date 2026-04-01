#pragma once

#include "LEDAnimations.hpp"

namespace LED {
    int init();
    /// @brief set the current animation that the led thread is playing
    /// @param animation a pointer to a valid animation defined in LEDAnimations.hpp 
    /// @return true if the animation was set, false on error
    bool set_animation(const Animation::Animation *animation);
};
