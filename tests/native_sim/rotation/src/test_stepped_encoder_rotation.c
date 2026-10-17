// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Rotation tests, real stepped encoder driver on emulated GPIO. */

#include <stdint.h>

#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include <zmk/sensor/stepped_encoder.h>

#include "encoder_pins.h"

#define JUMP_EDGES 2

static void drain_rotation(void *fixture) {
    ARG_UNUSED(fixture);
    k_msleep(ENCODER_SETTLE_MS);
    (void)encoder_read_rotation();
}

ZTEST(stepped_encoder_rotation, test_clockwise_reads_positive) {
    encoder_rotate_clockwise();
    struct sensor_value rotation = encoder_read_rotation();
    zassert_equal(rotation.val1, ENCODER_CYCLE_DEGREES, "clockwise cycle reads positive degrees");
    zassert_equal(rotation.val2, 0, "clockwise cycle reads whole degrees");
}

ZTEST(stepped_encoder_rotation, test_counter_clockwise_reads_negative) {
    encoder_rotate_counter_clockwise();
    struct sensor_value rotation = encoder_read_rotation();
    zassert_equal(rotation.val1, -ENCODER_CYCLE_DEGREES,
                  "counter-clockwise cycle reads negative degrees");
    zassert_equal(rotation.val2, 0, "counter-clockwise cycle reads whole degrees");
}

ZTEST(stepped_encoder_rotation, test_two_state_jump_is_rejected) {
    struct stepped_encoder_statistics before = {0};
    stepped_encoder_statistics_get(encoder_device(), &before);
    encoder_set_both_channels(1);
    encoder_set_both_channels(0);
    struct stepped_encoder_statistics after = {0};
    stepped_encoder_statistics_get(encoder_device(), &after);
    zassert_equal(after.rejected - before.rejected, JUMP_EDGES, "both jumps rejected");
    zassert_equal(after.steps_clockwise, before.steps_clockwise, "jump makes no clockwise step");
    zassert_equal(after.steps_counter_clockwise, before.steps_counter_clockwise,
                  "jump makes no counter-clockwise step");
    struct sensor_value rotation = encoder_read_rotation();
    zassert_equal(rotation.val1, 0, "jump reads no rotation");
    zassert_equal(rotation.val2, 0, "jump reads no partial rotation");
}

ZTEST(stepped_encoder_rotation, test_rotation_after_idle_counts) {
    encoder_rotate_clockwise();
    k_msleep(ENCODER_SETTLE_MS);
    (void)encoder_read_rotation();
    encoder_rotate_clockwise();
    struct sensor_value rotation = encoder_read_rotation();
    zassert_equal(rotation.val1, ENCODER_CYCLE_DEGREES, "rotation after idle counts");
}

ZTEST_SUITE(stepped_encoder_rotation, NULL, NULL, drain_rotation, NULL, NULL);
