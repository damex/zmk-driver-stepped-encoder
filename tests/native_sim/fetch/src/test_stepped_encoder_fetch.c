// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Fetch tests, real stepped encoder driver on emulated GPIO. */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/ztest.h>

#include "encoder_pins.h"

ZTEST(stepped_encoder_fetch, test_repeated_read_after_one_fetch_matches) {
    const struct device *encoder = encoder_device();
    encoder_rotate_clockwise();
    zassert_ok(sensor_sample_fetch(encoder), "sample fetch");
    struct sensor_value first = {0};
    zassert_ok(sensor_channel_get(encoder, SENSOR_CHAN_ROTATION, &first), "first read");
    struct sensor_value second = {0};
    zassert_ok(sensor_channel_get(encoder, SENSOR_CHAN_ROTATION, &second), "second read");
    zassert_equal(first.val1, ENCODER_CYCLE_DEGREES, "first read sees fetched rotation");
    zassert_equal(second.val1, first.val1, "second read matches first");
    zassert_equal(second.val2, first.val2, "second read matches first partial rotation");
}

ZTEST(stepped_encoder_fetch, test_unsupported_channel_fetch_is_refused) {
    zassert_equal(sensor_sample_fetch_chan(encoder_device(), SENSOR_CHAN_ACCEL_X), -ENOTSUP,
                  "unsupported channel fetch refused");
}

ZTEST_SUITE(stepped_encoder_fetch, NULL, NULL, NULL, NULL, NULL);
