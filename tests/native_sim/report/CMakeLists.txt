# Copyright 2026 Roman Kuzmitskii (@damex)
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.20.0)

include(${CMAKE_CURRENT_SOURCE_DIR}/../common/native_sim.cmake)
project(stepped_encoder_report_test)

target_sources(app PRIVATE
  src/test_stepped_encoder_report.c
)
