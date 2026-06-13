# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 H2Lab Development Team

libdrvcan = static_library('drvcan',
    sources: [
        'stm32u5_fdcan_driver.c',
        'stm32u5_fdcan_driver.h',
    ],
    include_directories: include_directories('.'),
    dependencies: [ merlin_dep ],
)

drvcan_dep = declare_dependency(
    link_with: libdrvcan,
    include_directories: include_directories('.'),
)
