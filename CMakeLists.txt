# Copyright 2026 Roman Kuzmitskii (@damex)
# SPDX-License-Identifier: MIT

if(CONFIG_SENSOR_STEPPED_ENCODER)
  zephyr_include_directories(include)
  zephyr_library_named(zmk_driver_stepped_encoder)
  zephyr_library_compile_options(
      -Werror
      -Wextra
      -Wshadow
      -Wformat=2
      -Wlogical-op
      -Wduplicated-cond
      -Wmissing-prototypes
      -Wjump-misses-init
      -Wwrite-strings
      -Winit-self
      -Wold-style-definition
      -fanalyzer
  )
  zephyr_library_sources(src/stepped_encoder.c)
  zephyr_library_sources(src/stepped_encoder_decode.c)
endif()
