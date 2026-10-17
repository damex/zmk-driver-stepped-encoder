// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Report coalescing tests, real stepped encoder driver on emulated GPIO. */

#include <stdint.h>

#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#include "encoder_pins.h"

/* Same window as driver report coalescing. */
#define COALESCE_WINDOW_MS 8

static atomic_t report_calls;

static void count_report(const struct device *dev, const struct sensor_trigger *trigger) {
    ARG_UNUSED(dev);
    ARG_UNUSED(trigger);
    atomic_inc(&report_calls);
}

static void *bind_report_counter(void) {
    zassert_ok(sensor_trigger_set(encoder_device(), &encoder_rotation_trigger, count_report),
               "trigger binds");
    return NULL;
}

ZTEST(stepped_encoder_report, test_steps_within_window_report_once) {
    int64_t start_ms = k_uptime_get();
    encoder_rotate_clockwise();
    zassert_true(k_uptime_get() - start_ms < COALESCE_WINDOW_MS,
                 "cycle lands inside coalesce window");
    k_msleep(ENCODER_SETTLE_MS);
    zassert_equal(atomic_get(&report_calls), 1, "one report for whole cycle");
}

ZTEST_SUITE(stepped_encoder_report, NULL, bind_report_counter, NULL, NULL, NULL);
