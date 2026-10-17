// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Trigger binding tests, real stepped encoder driver on emulated GPIO. */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include "encoder_pins.h"

static atomic_t first_handler_calls;
static atomic_t second_handler_calls;

static void first_handler(const struct device *dev, const struct sensor_trigger *trigger) {
    ARG_UNUSED(dev);
    ARG_UNUSED(trigger);
    atomic_inc(&first_handler_calls);
}

static void second_handler(const struct device *dev, const struct sensor_trigger *trigger) {
    ARG_UNUSED(dev);
    ARG_UNUSED(trigger);
    atomic_inc(&second_handler_calls);
}

ZTEST(stepped_encoder_trigger, test_second_trigger_set_is_refused) {
    const struct device *encoder = encoder_device();
    zassert_true(device_is_ready(encoder), "encoder ready");
    zassert_ok(sensor_trigger_set(encoder, &encoder_rotation_trigger, first_handler),
               "first trigger set binds");
    zassert_equal(sensor_trigger_set(encoder, &encoder_rotation_trigger, second_handler),
                  -EALREADY, "second trigger set refused");
    encoder_rotate_clockwise();
    k_msleep(ENCODER_SETTLE_MS);
    zassert_true(atomic_get(&first_handler_calls) > 0, "first handler fires on rotation");
    zassert_equal(atomic_get(&second_handler_calls), 0, "second handler never fires");
}

ZTEST_SUITE(stepped_encoder_trigger, NULL, NULL, NULL, NULL, NULL);
