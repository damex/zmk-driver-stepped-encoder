// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

#pragma once

#include <stdint.h>

#define STEPPED_ENCODER_FULL_ROTATION 360
#define STEPPED_ENCODER_MICRODEGREES_PER_DEGREE 1000000

struct stepped_encoder_rotation {
    int32_t degrees;
    int32_t microdegrees;
};

int8_t stepped_encoder_decode_step(uint8_t previous_state, uint8_t new_state);

void stepped_encoder_decode_rotation(int16_t pulses, uint16_t steps,
                                     struct stepped_encoder_rotation *rotation);
