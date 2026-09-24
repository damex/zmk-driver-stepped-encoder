// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

#define DT_DRV_COMPAT zmk_stepped_encoder

#include <zmk/sensor/stepped_encoder.h>

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/irq.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/__assert.h>
#include <zephyr/sys/util.h>

#include "stepped_encoder_decode.h"

LOG_MODULE_REGISTER(stepped_encoder, CONFIG_SENSOR_STEPPED_ENCODER_LOG_LEVEL);

#define REPORT_COALESCE_MS 8
#define POLL_INTERVAL_US 150
#define IDLE_STABLE_POLLS 32
#define STATS_LOG_INTERVAL_SEC 2

/* K_USEC rounds up to one tick, so 150 us silently becomes the tick period on
 * slower kernels. nRF ZMK builds run at 32768 Hz, giving 30.5 us per tick. */
BUILD_ASSERT(CONFIG_SYS_CLOCK_TICKS_PER_SEC * POLL_INTERVAL_US >= 2 * USEC_PER_SEC,
             "SYS_CLOCK_TICKS_PER_SEC too low for POLL_INTERVAL_US");

struct stepped_encoder_config {
    const struct gpio_dt_spec a;
    const struct gpio_dt_spec b;
    const uint16_t steps;
};

struct stepped_encoder_data {
    const struct device *dev;

    uint8_t ab_state;
    uint16_t poll_stable;
    int16_t pulses;

    struct gpio_callback a_gpio_cb;
    struct gpio_callback b_gpio_cb;
    struct k_timer poll_timer;
    struct k_work_delayable report_work;

    sensor_trigger_handler_t handler;
    const struct sensor_trigger *trigger;

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATS)
    /* Written from poll timer, read lock-free by stats. */
    volatile uint32_t steps_cw;
    volatile uint32_t steps_ccw;
    volatile uint32_t rejected;
    struct k_work_delayable stats_work;
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

static int stepped_encoder_arm(const struct stepped_encoder_config *config, bool enable) {
    gpio_flags_t mode = enable ? GPIO_INT_EDGE_BOTH : GPIO_INT_DISABLE;

    int error = gpio_pin_interrupt_configure_dt(&config->a, mode);
    if (error < 0) {
        return error;
    }
    return gpio_pin_interrupt_configure_dt(&config->b, mode);
}

static void stepped_encoder_wake(struct stepped_encoder_data *data) {
    const struct stepped_encoder_config *config = data->dev->config;

    (void)stepped_encoder_arm(config, false);
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
            bool arm_failed = stepped_encoder_arm(config, true) < 0;
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

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATS)
    if (step > 0) {
        data->steps_cw++;
    } else if (step < 0) {
        data->steps_ccw++;
    } else {
        data->rejected++;
    }
    k_work_schedule(&data->stats_work, K_SECONDS(STATS_LOG_INTERVAL_SEC));
#endif

    if (step == 0) {
        return;
    }

    data->pulses += step;
    k_work_schedule(&data->report_work, K_MSEC(REPORT_COALESCE_MS));
}

static void stepped_encoder_a_gpio_callback(const struct device *port, struct gpio_callback *cb,
                                            uint32_t pins) {
    struct stepped_encoder_data *data = CONTAINER_OF(cb, struct stepped_encoder_data, a_gpio_cb);

    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    stepped_encoder_wake(data);
}

static void stepped_encoder_b_gpio_callback(const struct device *port, struct gpio_callback *cb,
                                            uint32_t pins) {
    struct stepped_encoder_data *data = CONTAINER_OF(cb, struct stepped_encoder_data, b_gpio_cb);

    ARG_UNUSED(port);
    ARG_UNUSED(pins);

    stepped_encoder_wake(data);
}

static void stepped_encoder_report_work_cb(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct stepped_encoder_data *data =
        CONTAINER_OF(dwork, struct stepped_encoder_data, report_work);

    /* trigger_set swaps handler and trigger as a pair. */
    unsigned int key = irq_lock();
    sensor_trigger_handler_t handler = data->handler;
    const struct sensor_trigger *trigger = data->trigger;
    irq_unlock(key);

    if (handler != NULL) {
        handler(data->dev, trigger);
    }
}

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATS)
static void stepped_encoder_stats_log(struct k_work *work) {
    struct k_work_delayable *dwork = k_work_delayable_from_work(work);
    struct stepped_encoder_data *data =
        CONTAINER_OF(dwork, struct stepped_encoder_data, stats_work);

    struct stepped_encoder_stats stats;
    stepped_encoder_stats_get(data->dev, &stats);
    uint32_t rejected = stats.edges_seen - stats.steps_cw - stats.steps_ccw;
    LOG_INF("%s edges=%u cw=%u ccw=%u rejected=%u", data->dev->name, stats.edges_seen,
            stats.steps_cw, stats.steps_ccw, rejected);
}

void stepped_encoder_stats_get(const struct device *dev, struct stepped_encoder_stats *out) {
    __ASSERT_NO_MSG(dev != NULL);
    __ASSERT_NO_MSG(out != NULL);

    struct stepped_encoder_data *data = dev->data;

    out->steps_cw = data->steps_cw;
    out->steps_ccw = data->steps_ccw;
    out->edges_seen = out->steps_cw + out->steps_ccw + data->rejected;
}
#endif

static int stepped_encoder_sample_fetch(const struct device *dev, enum sensor_channel chan) {
    __ASSERT_NO_MSG(chan == SENSOR_CHAN_ALL || chan == SENSOR_CHAN_ROTATION);

    ARG_UNUSED(dev);
    ARG_UNUSED(chan);

    return 0;
}

static int stepped_encoder_channel_get(const struct device *dev, enum sensor_channel chan,
                                       struct sensor_value *val) {
    struct stepped_encoder_data *data = dev->data;
    const struct stepped_encoder_config *config = dev->config;

    if (chan != SENSOR_CHAN_ROTATION) {
        return -ENOTSUP;
    }

    /* Exclude the poll writer across this read-reset. */
    unsigned int key = irq_lock();
    int16_t pulses = data->pulses;
    data->pulses = 0;
    irq_unlock(key);

    struct stepped_encoder_rotation rotation;
    stepped_encoder_decode_rotation(pulses, config->steps, &rotation);
    val->val1 = rotation.degrees;
    val->val2 = rotation.microdegrees;

    return 0;
}

static int stepped_encoder_trigger_set(const struct device *dev, const struct sensor_trigger *trig,
                                       sensor_trigger_handler_t handler) {
    struct stepped_encoder_data *data = dev->data;

    unsigned int key = irq_lock();
    data->trigger = trig;
    data->handler = handler;
    /* Drop motion accumulated before the listener attached. */
    data->pulses = 0;
    irq_unlock(key);

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
    k_work_init_delayable(&data->report_work, stepped_encoder_report_work_cb);

#if defined(CONFIG_SENSOR_STEPPED_ENCODER_STATS)
    k_work_init_delayable(&data->stats_work, stepped_encoder_stats_log);
#endif

    gpio_init_callback(&data->a_gpio_cb, stepped_encoder_a_gpio_callback, BIT(config->a.pin));
    if (gpio_add_callback(config->a.port, &data->a_gpio_cb) < 0) {
        LOG_ERR("A callback add failed");
        return -EIO;
    }
    gpio_init_callback(&data->b_gpio_cb, stepped_encoder_b_gpio_callback, BIT(config->b.pin));
    if (gpio_add_callback(config->b.port, &data->b_gpio_cb) < 0) {
        LOG_ERR("B callback add failed");
        return -EIO;
    }

    int initial_state = stepped_encoder_read_state(config);
    if (initial_state < 0) {
        LOG_ERR("initial state read failed");
        return -EIO;
    }
    data->ab_state = (uint8_t)initial_state;

    int error = stepped_encoder_arm(config, true);
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
