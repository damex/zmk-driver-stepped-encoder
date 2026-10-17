// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Stepped encoder public API, decode counters. */
#pragma once

#include <stdint.h>

#include <zephyr/device.h>

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATISTICS)

struct stepped_encoder_statistics {
    uint32_t edges_seen;
    uint32_t steps_clockwise;
    uint32_t steps_counter_clockwise;
};

void stepped_encoder_statistics_get(const struct device *dev,
                                    struct stepped_encoder_statistics *out);

#endif
