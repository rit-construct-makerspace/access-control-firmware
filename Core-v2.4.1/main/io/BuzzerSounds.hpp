#pragma once

#include <cstdint>

namespace SoundEffect {
    struct Note {
        uint32_t frequency;
        uint16_t duration; // in MS
    };

    struct Effect {
        uint16_t length;
        const Note* notes;
    };

    /*******************************
     * BEGIN NOTE DEFS
     ******************************/

    const Note LOW_TONE = {
        .frequency = 2000,
        .duration = 250,
    };

    const Note HIGH_TONE = {
        .frequency = 4000,
        .duration = 250,
    };

    /********************************
     * BEGIN EFFECTS
     *******************************/

    const Note ACCEPTED_NOTES[2] = {
        LOW_TONE,
        HIGH_TONE
    };

    const Effect ACCEPTED_EFFECT = {
        .length = 2,
        .notes = ACCEPTED_NOTES,
    };
}