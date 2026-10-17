// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Emulated GPIO drive of shared overlay encoder, for native_sim suites. */

#include "encoder_pins.h"

#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#define EDGE_GAP_MS 1

BUILD_ASSERT((ENCODER_PULSES_PER_CYCLE * ENCODER_FULL_ROTATION_DEGREES) %
                     DT_PROP(ENCODER_NODE, steps) ==
                 0,
             "one cycle maps to whole degrees");

const struct sensor_trigger encoder_rotation_trigger = {
    .type = SENSOR_TRIG_DATA_READY,
    .chan = SENSOR_CHAN_ROTATION,
};

static const struct gpio_dt_spec channel_a = GPIO_DT_SPEC_GET(ENCODER_NODE, a_gpios);
static const struct gpio_dt_spec channel_b = GPIO_DT_SPEC_GET(ENCODER_NODE, b_gpios);

const struct device *encoder_device(void) {
    return DEVICE_DT_GET(ENCODER_NODE);
}

static void drive_channel(const struct gpio_dt_spec *channel, int level) {
    zassert_ok(gpio_emul_input_set(channel->port, channel->pin, level), "pin level set");
}

static void step_channel(const struct gpio_dt_spec *channel, int level) {
    drive_channel(channel, level);
    k_msleep(EDGE_GAP_MS);
}

void encoder_rotate_clockwise(void) {
    step_channel(&channel_b, 1);
    step_channel(&channel_a, 1);
    step_channel(&channel_b, 0);
    step_channel(&channel_a, 0);
}

void encoder_rotate_counter_clockwise(void) {
    step_channel(&channel_a, 1);
    step_channel(&channel_b, 1);
    step_channel(&channel_a, 0);
    step_channel(&channel_b, 0);
}

void encoder_set_both_channels(int level) {
    drive_channel(&channel_a, level);
    drive_channel(&channel_b, level);
    k_msleep(EDGE_GAP_MS);
}

struct sensor_value encoder_read_rotation(void) {
    struct sensor_value rotation = {0};
    zassert_ok(sensor_sample_fetch(encoder_device()), "sample fetch");
    zassert_ok(sensor_channel_get(encoder_device(), SENSOR_CHAN_ROTATION, &rotation),
               "rotation read");
    return rotation;
}
