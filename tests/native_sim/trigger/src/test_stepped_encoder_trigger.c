// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Trigger binding tests, real stepped encoder driver on emulated GPIO. */

#include <errno.h>

#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/gpio/gpio_emul.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/atomic.h>
#include <zephyr/sys/util.h>
#include <zephyr/ztest.h>

#define ENCODER_NODE DT_NODELABEL(encoder)
#define EDGE_GAP_MS 1
#define REPORT_WAIT_MS 50

static const struct device *const encoder = DEVICE_DT_GET(ENCODER_NODE);
static const struct gpio_dt_spec channel_a = GPIO_DT_SPEC_GET(ENCODER_NODE, a_gpios);
static const struct gpio_dt_spec channel_b = GPIO_DT_SPEC_GET(ENCODER_NODE, b_gpios);
static const struct sensor_trigger rotation_trigger = {
    .type = SENSOR_TRIG_DATA_READY,
    .chan = SENSOR_CHAN_ROTATION,
};

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

static void set_channel(const struct gpio_dt_spec *channel, int level) {
    zassert_ok(gpio_emul_input_set(channel->port, channel->pin, level), "pin level set");
    k_msleep(EDGE_GAP_MS);
}

static void rotate_clockwise(void) {
    set_channel(&channel_b, 1);
    set_channel(&channel_a, 1);
    set_channel(&channel_b, 0);
    set_channel(&channel_a, 0);
}

ZTEST(stepped_encoder_trigger, test_second_trigger_set_is_refused) {
    zassert_true(device_is_ready(encoder), "encoder ready");
    zassert_ok(sensor_trigger_set(encoder, &rotation_trigger, first_handler),
               "first trigger set binds");
    zassert_equal(sensor_trigger_set(encoder, &rotation_trigger, second_handler), -EALREADY,
                  "second trigger set refused");
    rotate_clockwise();
    k_msleep(REPORT_WAIT_MS);
    zassert_true(atomic_get(&first_handler_calls) > 0, "first handler fires on rotation");
    zassert_equal(atomic_get(&second_handler_calls), 0, "second handler never fires");
}

ZTEST_SUITE(stepped_encoder_trigger, NULL, NULL, NULL, NULL, NULL);
