# Copyright 2026 Roman Kuzmitskii (@damex)
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20.0)

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_SOURCE_DIR}/../../..)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(stepped_encoder_trigger_test)

target_sources(app PRIVATE
  src/test_stepped_encoder_trigger.c
)
