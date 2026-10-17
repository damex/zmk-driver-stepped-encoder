# Copyright 2026 Roman Kuzmitskii (@damex)
# SPDX-License-Identifier: MIT

list(APPEND ZEPHYR_EXTRA_MODULES ${CMAKE_CURRENT_LIST_DIR}/../../..)
set(CONF_FILE ${CMAKE_CURRENT_LIST_DIR}/prj.conf)
set(DTC_OVERLAY_FILE ${CMAKE_CURRENT_LIST_DIR}/encoder.overlay)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

target_include_directories(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/include
)

target_sources(app PRIVATE
  ${CMAKE_CURRENT_LIST_DIR}/src/encoder_pins.c
)
