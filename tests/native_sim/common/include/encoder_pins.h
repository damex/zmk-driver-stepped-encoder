// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Emulated GPIO drive of shared overlay encoder, for native_sim suites. */
#pragma once

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/sensor.h>

#define ENCODER_NODE DT_NODELABEL(encoder)
#define ENCODER_PULSES_PER_CYCLE 4
#define ENCODER_FULL_ROTATION_DEGREES 360
#define ENCODER_CYCLE_DEGREES                                                                      \
    (ENCODER_PULSES_PER_CYCLE * ENCODER_FULL_ROTATION_DEGREES / DT_PROP(ENCODER_NODE, steps))
#define ENCODER_SETTLE_MS 20

extern const struct sensor_trigger encoder_rotation_trigger;

const struct device *encoder_device(void);

void encoder_rotate_clockwise(void);

void encoder_rotate_counter_clockwise(void);

/* Both channels change before next poll, so poll sees two-state jump. */
void encoder_set_both_channels(int level);

struct sensor_value encoder_read_rotation(void);
