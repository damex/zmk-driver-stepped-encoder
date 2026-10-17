// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

/* Sensor driver for detented rotary encoders. */

#define DT_DRV_COMPAT zmk_stepped_encoder

#include <zmk/sensor/stepped_encoder.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

#include "stepped_encoder_decode.h"

LOG_MODULE_REGISTER(stepped_encoder, CONFIG_SENSOR_STEPPED_ENCODER_LOG_LEVEL);

#define REPORT_COALESCE_MS 8
#define POLL_INTERVAL_US 150
#define IDLE_STABLE_POLLS 32
#define STATISTICS_LOG_INTERVAL_SEC 2

/* K_USEC rounds up to whole ticks.
 * Two ticks or more per period keep poll rate near POLL_INTERVAL_US. */
BUILD_ASSERT(CONFIG_SYS_CLOCK_TICKS_PER_SEC * POLL_INTERVAL_US >= 2 * USEC_PER_SEC,
             "SYS_CLOCK_TICKS_PER_SEC too low for POLL_INTERVAL_US");

enum stepped_encoder_binding {
    STEPPED_ENCODER_BINDING_FREE,
    STEPPED_ENCODER_BINDING_WRITING,
    STEPPED_ENCODER_BINDING_READY,
};

struct stepped_encoder_config {
    const struct gpio_dt_spec a;
    const struct gpio_dt_spec b;
    const uint16_t steps;
};

struct stepped_encoder_data {
    const struct device *dev;

    uint8_t ab_state;
    uint16_t poll_stable;
    atomic_t pulses;
    struct stepped_encoder_rotation fetched_rotation;

    struct gpio_callback a_gpio_callback;
    struct gpio_callback b_gpio_callback;
    struct k_timer poll_timer;
    struct k_work_delayable report_work;

    sensor_trigger_handler_t handler;
    const struct sensor_trigger *trigger;
    atomic_t binding;

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATISTICS)
    /* Written from poll timer, read lock-free by statistics getter. */
    volatile uint32_t steps_clockwise;
    volatile uint32_t steps_counter_clockwise;
    volatile uint32_t rejected;
    struct k_work_delayable statistics_work;
#endif
};

static int stepped_encoder_read_state(const struct stepped_encoder_config *config) {
    int a_level = gpio_pin_get_dt(&config->a);
    if (a_level < 0) {
        return a_level;
    }
    int b_level = gpio_pin_get_dt(&config->b);
    if (b_level < 0) {
        return b_level;
    }
    return (a_level << 1) | b_level;
}

static int stepped_encoder_set_interrupts(const struct stepped_encoder_config *config,
                                          gpio_flags_t mode) {
    int error = gpio_pin_interrupt_configure_dt(&config->a, mode);
    if (error < 0) {
        return error;
    }
    return gpio_pin_interrupt_configure_dt(&config->b, mode);
}

static void stepped_encoder_wake(struct stepped_encoder_data *data) {
    const struct stepped_encoder_config *config = data->dev->config;

    (void)stepped_encoder_set_interrupts(config, GPIO_INT_DISABLE);
    data->poll_stable = 0;
    k_timer_start(&data->poll_timer, K_USEC(POLL_INTERVAL_US), K_USEC(POLL_INTERVAL_US));
}

static void stepped_encoder_poll(struct k_timer *timer) {
    struct stepped_encoder_data *data =
        CONTAINER_OF(timer, struct stepped_encoder_data, poll_timer);
    const struct stepped_encoder_config *config = data->dev->config;

    int sample = stepped_encoder_read_state(config);
    if (sample < 0) {
        return;
    }
    uint8_t new_state = (uint8_t)sample;

    if (new_state == data->ab_state) {
        data->poll_stable++;
        if (data->poll_stable >= IDLE_STABLE_POLLS) {
            k_timer_stop(&data->poll_timer);
            bool arm_failed = stepped_encoder_set_interrupts(config, GPIO_INT_EDGE_BOTH) < 0;
            /* Re-read closes the lost-edge window between timer stop and arm. */
            int current_state = stepped_encoder_read_state(config);
            bool read_failed = current_state < 0;
            bool edge_missed = current_state != data->ab_state;
            if (arm_failed || read_failed || edge_missed) {
                stepped_encoder_wake(data);
            }
        }
        return;
    }

    int8_t step = stepped_encoder_decode_step(data->ab_state, new_state);
    data->ab_state = new_state;
    data->poll_stable = 0;

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATISTICS)
    if (step > 0) {
        data->steps_clockwise++;
    } else if (step < 0) {
        data->steps_counter_clockwise++;
    } else {
        data->rejected++;
    }
    k_work_schedule(&data->statistics_work, K_SECONDS(STATISTICS_LOG_INTERVAL_SEC));
#endif

    if (step == 0) {
        return;
    }

    atomic_add(&data->pulses, step);
    k_work_schedule(&data->report_work, K_MSEC(REPORT_COALESCE_MS));
}

static void stepped_encoder_a_gpio_callback(const struct device *port, struct gpio_callback *cb,
                                            uint32_t pins) {
    struct stepped_encoder_data *data =
        CONTAINER_OF(cb, struct stepped_encoder_data, a_gpio_callback);

    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    stepped_encoder_wake(data);
}

static void stepped_encoder_b_gpio_callback(const struct device *port, struct gpio_callback *cb,
                                            uint32_t pins) {
    struct stepped_encoder_data *data =
        CONTAINER_OF(cb, struct stepped_encoder_data, b_gpio_callback);

    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    stepped_encoder_wake(data);
}

static void stepped_encoder_report_work_handler(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct stepped_encoder_data *data =
        CONTAINER_OF(delayable, struct stepped_encoder_data, report_work);

    if (atomic_get(&data->binding) != STEPPED_ENCODER_BINDING_READY) {
        return;
    }
    if (data->handler != NULL) {
        data->handler(data->dev, data->trigger);
    }
}

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATISTICS)
static void stepped_encoder_statistics_log(struct k_work *work) {
    struct k_work_delayable *delayable = k_work_delayable_from_work(work);
    struct stepped_encoder_data *data =
        CONTAINER_OF(delayable, struct stepped_encoder_data, statistics_work);

    struct stepped_encoder_statistics statistics;
    stepped_encoder_statistics_get(data->dev, &statistics);
    uint32_t steps = statistics.steps_clockwise + statistics.steps_counter_clockwise;
    uint32_t rejected = statistics.edges_seen - steps;
    LOG_INF("%s edges=%u clockwise=%u counter_clockwise=%u rejected=%u", data->dev->name,
            statistics.edges_seen, statistics.steps_clockwise,
            statistics.steps_counter_clockwise, rejected);
}

void stepped_encoder_statistics_get(const struct device *dev,
                                    struct stepped_encoder_statistics *out) {
    __ASSERT_NO_MSG(dev != NULL);
    __ASSERT_NO_MSG(out != NULL);

    struct stepped_encoder_data *data = dev->data;

    out->steps_clockwise = data->steps_clockwise;
    out->steps_counter_clockwise = data->steps_counter_clockwise;
    out->edges_seen = out->steps_clockwise + out->steps_counter_clockwise + data->rejected;
}
#endif

static int stepped_encoder_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    struct stepped_encoder_data *data = dev->data;
    const struct stepped_encoder_config *config = dev->config;

    if (chan != SENSOR_CHAN_ALL && chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    atomic_val_t raw_pulses = atomic_set(&data->pulses, 0);
    int16_t pulses = (int16_t)CLAMP(raw_pulses, INT16_MIN, INT16_MAX);
    stepped_encoder_decode_rotation(pulses, config->steps, &data->fetched_rotation);

    return 0;
}

static int stepped_encoder_channel_get(const struct device *dev, enum sensor_channel chan,
                                       struct sensor_value *val) {
    const struct stepped_encoder_data *data = dev->data;

    if (chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    val->val1 = data->fetched_rotation.degrees;
    val->val2 = data->fetched_rotation.microdegrees;

    return 0;
}

static int stepped_encoder_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
                                       sensor_trigger_handler_t handler) {
    struct stepped_encoder_data *data = dev->data;

    if (!atomic_cas(&data->binding, STEPPED_ENCODER_BINDING_FREE,
                    STEPPED_ENCODER_BINDING_WRITING)) {
        return -EALREADY;
    }
    data->handler = handler;
    data->trigger = trig;
    /* Motion from before bind never reports. */
    (void)atomic_set(&data->pulses, 0);
    /* Report work never sees partial binding. */
    (void)atomic_set(&data->binding, STEPPED_ENCODER_BINDING_READY);

    return 0;
}

static const struct sensor_driver_api stepped_encoder_api = {
    .trigger_set = stepped_encoder_trigger_set,
    .sample_fetch = stepped_encoder_sample_fetch,
    .channel_get = stepped_encoder_channel_get,
};

static int stepped_encoder_init(const struct device *dev) {
    struct stepped_encoder_data *data = dev->data;
    const struct stepped_encoder_config *config = dev->config;

    if (!device_is_ready(config->a.port)) {
        LOG_ERR("A GPIO not ready");
        return -ENODEV;
    }
    if (!device_is_ready(config->b.port)) {
        LOG_ERR("B GPIO not ready");
        return -ENODEV;
    }

    if (gpio_pin_configure_dt(&config->a, GPIO_INPUT) < 0) {
        LOG_ERR("A pin configure failed");
        return -EIO;
    }
    if (gpio_pin_configure_dt(&config->b, GPIO_INPUT) < 0) {
        LOG_ERR("B pin configure failed");
        return -EIO;
    }

    data->dev = dev;
    k_timer_init(&data->poll_timer, stepped_encoder_poll, NULL);
    k_work_init_delayable(&data->report_work, stepped_encoder_report_work_handler);

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATISTICS)
    k_work_init_delayable(&data->statistics_work, stepped_encoder_statistics_log);
#endif

    gpio_init_callback(&data->a_gpio_callback, stepped_encoder_a_gpio_callback, BIT(config->a.pin));
    if (gpio_add_callback(config->a.port, &data->a_gpio_callback) < 0) {
        LOG_ERR("A callback add failed");
        return -EIO;
    }
    gpio_init_callback(&data->b_gpio_callback, stepped_encoder_b_gpio_callback, BIT(config->b.pin));
    if (gpio_add_callback(config->b.port, &data->b_gpio_callback) < 0) {
        LOG_ERR("B callback add failed");
        return -EIO;
    }

    int initial_state = stepped_encoder_read_state(config);
    if (initial_state < 0) {
        LOG_ERR("initial state read failed");
        return -EIO;
    }
    data->ab_state = (uint8_t)initial_state;

    int error = stepped_encoder_set_interrupts(config, GPIO_INT_EDGE_BOTH);
    if (error < 0) {
        LOG_ERR("interrupt configure failed (%d)", error);
        return error;
    }

    return 0;
}

#define STEPPED_ENCODER_INST(instance)                                                             \
    BUILD_ASSERT(DT_INST_PROP(instance, steps) > 0, "zmk,stepped-encoder steps must be > 0");      \
    BUILD_ASSERT(DT_INST_PROP(instance, steps) <= STEPPED_ENCODER_FULL_ROTATION,                   \
                 "zmk,stepped-encoder steps must be <= 360");                                      \
    static struct stepped_encoder_data stepped_encoder_data_##instance;                            \
    static const struct stepped_encoder_config stepped_encoder_config_##instance = {               \
        .a = GPIO_DT_SPEC_INST_GET(instance, a_gpios),                                             \
        .b = GPIO_DT_SPEC_INST_GET(instance, b_gpios),                                             \
        .steps = DT_INST_PROP(instance, steps),                                                    \
    };                                                                                             \
    DEVICE_DT_INST_DEFINE(instance, stepped_encoder_init, NULL,                                    \
                          &stepped_encoder_data_##instance, &stepped_encoder_config_##instance,    \
                          POST_KERNEL, CONFIG_SENSOR_INIT_PRIORITY, &stepped_encoder_api);

DT_INST_FOREACH_STATUS_OKAY(STEPPED_ENCODER_INST)
