// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Bind tests, real stepped encoder driver on emulated GPIO. */

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <zmk/sensor/stepped_encoder.h>

#include "encoder_pins.h"

static void ignore_report(const struct device *dev, const struct sensor_trigger *trigger) {
    ARG_UNUSED(dev);
    ARG_UNUSED(trigger);
}

ZTEST(stepped_encoder_bind, test_motion_before_bind_reads_zero) {
    encoder_rotate_clockwise();
    k_msleep(ENCODER_SETTLE_MS);
    struct stepped_encoder_statistics statistics = {0};
    stepped_encoder_statistics_get(encoder_device(), &statistics);
    zassert_equal(statistics.steps_clockwise, ENCODER_PULSES_PER_CYCLE, "cycle seen before bind");
    zassert_ok(sensor_trigger_set(encoder_device(), &encoder_rotation_trigger, ignore_report),
               "trigger binds");
    struct sensor_value rotation = encoder_read_rotation();
    zassert_equal(rotation.val1, 0, "motion before bind reads no rotation");
    zassert_equal(rotation.val2, 0, "motion before bind reads no partial rotation");
}

ZTEST_SUITE(stepped_encoder_bind, NULL, NULL, NULL, NULL, NULL);
