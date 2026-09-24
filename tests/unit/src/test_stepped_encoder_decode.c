// Copyright 2026 Roman Kuzmitskii (@damex)
// SPDX-License-Identifier: MIT

#include <zephyr/ztest.h>

#include "stepped_encoder_decode.h"

ZTEST_SUITE(stepped_encoder_decode, NULL, NULL, NULL, NULL, NULL);

ZTEST(stepped_encoder_decode, test_forward_sequence_is_positive) {
    zassert_equal(stepped_encoder_decode_step(0b00, 0b01), 1, "00->01 cw");
    zassert_equal(stepped_encoder_decode_step(0b01, 0b11), 1, "01->11 cw");
    zassert_equal(stepped_encoder_decode_step(0b11, 0b10), 1, "11->10 cw");
    zassert_equal(stepped_encoder_decode_step(0b10, 0b00), 1, "10->00 cw");
}

ZTEST(stepped_encoder_decode, test_reverse_sequence_is_negative) {
    zassert_equal(stepped_encoder_decode_step(0b00, 0b10), -1, "00->10 ccw");
    zassert_equal(stepped_encoder_decode_step(0b10, 0b11), -1, "10->11 ccw");
    zassert_equal(stepped_encoder_decode_step(0b11, 0b01), -1, "11->01 ccw");
    zassert_equal(stepped_encoder_decode_step(0b01, 0b00), -1, "01->00 ccw");
}

ZTEST(stepped_encoder_decode, test_two_bit_jump_rejected) {
    zassert_equal(stepped_encoder_decode_step(0b00, 0b11), 0, "diagonal jump");
    zassert_equal(stepped_encoder_decode_step(0b11, 0b00), 0, "diagonal jump");
    zassert_equal(stepped_encoder_decode_step(0b01, 0b10), 0, "diagonal jump");
    zassert_equal(stepped_encoder_decode_step(0b10, 0b01), 0, "diagonal jump");
}

ZTEST(stepped_encoder_decode, test_no_transition_yields_zero) {
    zassert_equal(stepped_encoder_decode_step(0b00, 0b00), 0, "no change");
    zassert_equal(stepped_encoder_decode_step(0b01, 0b01), 0, "no change");
    zassert_equal(stepped_encoder_decode_step(0b10, 0b10), 0, "no change");
    zassert_equal(stepped_encoder_decode_step(0b11, 0b11), 0, "no change");
}

ZTEST(stepped_encoder_decode, test_high_state_bits_masked) {
    zassert_equal(stepped_encoder_decode_step(0xFC, 0xFD), 1, "high bits masked, 00->01 cw");
}

ZTEST(stepped_encoder_decode, test_bounce_pair_cancels) {
    int16_t net = 0;

    net += stepped_encoder_decode_step(0b00, 0b01);
    net += stepped_encoder_decode_step(0b01, 0b00);
    zassert_equal(net, 0, "cw then ccw cancels");
}

ZTEST(stepped_encoder_decode, test_ec11_20_detent_one_pulse) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(1, 80, &rotation);
    zassert_equal(rotation.degrees, 4, "1/80 rotation is 4 degrees whole");
    zassert_equal(rotation.microdegrees, 500000, "remainder is 0.5 degree");
}

ZTEST(stepped_encoder_decode, test_full_rotation_boundary) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(1, 360, &rotation);
    zassert_equal(rotation.degrees, 1, "steps=360 gives one degree per pulse");
    zassert_equal(rotation.microdegrees, 0, "no remainder at steps=360");
}

ZTEST(stepped_encoder_decode, test_negative_pulses_split) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(-1, 80, &rotation);
    zassert_equal(rotation.degrees, -4, "sign carries to whole degrees");
    zassert_equal(rotation.microdegrees, -500000, "sign carries to remainder");
}

ZTEST(stepped_encoder_decode, test_odd_steps_truncate_remainder) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(1, 7, &rotation);
    zassert_equal(rotation.degrees, 51, "360/7 is 51 degrees whole");
    zassert_equal(rotation.microdegrees, 428571, "remainder 3/7 degree truncates toward zero");
}

ZTEST(stepped_encoder_decode, test_bulk_pulses_do_not_overflow) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(INT16_MAX, 80, &rotation);
    int64_t reconstructed = (int64_t)rotation.degrees * STEPPED_ENCODER_MICRODEGREES_PER_DEGREE +
                            rotation.microdegrees;
    int64_t expected = (int64_t)INT16_MAX * STEPPED_ENCODER_FULL_ROTATION *
                       STEPPED_ENCODER_MICRODEGREES_PER_DEGREE / 80;
    zassert_equal(reconstructed, expected, "scaling holds at INT16_MAX pulses");
}

ZTEST(stepped_encoder_decode, test_bulk_negative_pulses_do_not_overflow) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(INT16_MIN, 7, &rotation);
    int64_t reconstructed = (int64_t)rotation.degrees * STEPPED_ENCODER_MICRODEGREES_PER_DEGREE +
                            rotation.microdegrees;
    int64_t expected = (int64_t)INT16_MIN * STEPPED_ENCODER_FULL_ROTATION *
                       STEPPED_ENCODER_MICRODEGREES_PER_DEGREE / 7;
    zassert_equal(reconstructed, expected, "scaling holds at INT16_MIN pulses");
}

ZTEST(stepped_encoder_decode, test_large_steps_remainder_needs_int64) {
    struct stepped_encoder_rotation rotation;

    stepped_encoder_decode_rotation(1000, 65535, &rotation);
    zassert_equal(rotation.degrees, 5, "360000/65535 is 5 degrees whole");
    zassert_equal(rotation.microdegrees, 493247, "remainder 32325 scaled past INT32_MAX");
}
