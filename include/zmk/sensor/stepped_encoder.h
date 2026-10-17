// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Stepped encoder public API, decode counters. */
#pragma once

#include <stdint.h>

#include <zephyr/device.h>

struct stepped_encoder_statistics {
    uint32_t steps_clockwise;
    uint32_t steps_counter_clockwise;
    uint32_t rejected;
};

void stepped_encoder_statistics_get(const struct device *device,
                                    struct stepped_encoder_statistics *out);
