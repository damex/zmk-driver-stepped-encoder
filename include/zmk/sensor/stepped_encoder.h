/*
 * Copyright 2026 Roman Kuzmitskii (@damex)
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>

#include <zephyr/device.h>

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATS)

struct stepped_encoder_stats {
    uint32_t edges_seen;
    uint32_t steps_cw;
    uint32_t steps_ccw;
};

void stepped_encoder_stats_get(const struct device *dev, struct stepped_encoder_stats *out);

#endif
