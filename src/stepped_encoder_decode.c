// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

#include "stepped_encoder_decode.h"

#include <assert.h>
#include <stddef.h>

#define AB_STATE_BITS 2
#define AB_STATE_MASK 0x03
#define TRANSITION_COUNT 16

/* Zero slots reject two-bit jumps. */
static const int8_t quadrature_step[TRANSITION_COUNT] = {
    [0b0001] = 1,  [0b0111] = 1,  [0b1110] = 1,  [0b1000] = 1,
    [0b0010] = -1, [0b1011] = -1, [0b1101] = -1, [0b0100] = -1,
};

int8_t stepped_encoder_decode_step(uint8_t previous_state, uint8_t new_state) {
    uint8_t transition = (uint8_t)(((previous_state & AB_STATE_MASK) << AB_STATE_BITS) |
                                   (new_state & AB_STATE_MASK));

    return quadrature_step[transition];
}

void stepped_encoder_decode_rotation(int16_t pulses, uint16_t steps,
                                     struct stepped_encoder_rotation *rotation) {
    assert(rotation != NULL);
    assert(steps > 0);

    int32_t scaled = (int32_t)pulses * STEPPED_ENCODER_FULL_ROTATION;

    rotation->degrees = scaled / steps;
    rotation->microdegrees = scaled % steps;
    if (rotation->microdegrees != 0) {
        /* int64: remainder scaling wraps int32 past 2148 steps. */
        rotation->microdegrees = (int32_t)(((int64_t)rotation->microdegrees *
                                            STEPPED_ENCODER_MICRODEGREES_PER_DEGREE) /
                                           steps);
    }
}
