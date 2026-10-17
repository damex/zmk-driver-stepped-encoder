# Copyright 2026 Roman Kuzmitskii (@damex)
# SPDX-License-Identifier: MIT

if(CONFIG_SENSOR_STEPPED_ENCODER)
  zephyr_include_directories(include)
  zephyr_library_named(zmk_driver_stepped_encoder)
  target_include_directories(${ZEPHYR_CURRENT_LIBRARY} SYSTEM PRIVATE
      ${ZEPHYR_BASE}/include
  )
  zephyr_library_compile_options(
      -Werror
      -Wextra
      -Wshadow
      -Wformat=2
      -Wformat-truncation=2
      -Wformat-overflow=2
      -Wformat-signedness
      -Wlogical-op
      -Wduplicated-cond
      -Wduplicated-branches
      -Wmissing-prototypes
      -Wmissing-declarations
      -Wjump-misses-init
      -Wwrite-strings
      -Winit-self
      -Wold-style-definition
      -Wswitch-enum
      -Wswitch-default
      -Wimplicit-fallthrough=5
      -Wnull-dereference
      -Wvla
      -Walloca
      -Wfloat-equal
      -Wshift-overflow=2
      -Wstringop-overflow=4
      -Warray-bounds=2
      -Wstrict-overflow=2
      -Wtrampolines
      -Wstack-usage=256
      -fanalyzer
  )
  zephyr_library_sources(src/stepped_encoder.c)
  zephyr_library_sources(src/stepped_encoder_decode.c)
endif()
