# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab Development Team

libdevili2130 = static_library('devili2130',
    sources: [
        'ili2130_driver.c',
    ],
    include_directories: include_directories('.'),
    dependencies: [ merlin_dep, drvi2c_dep ],
)

drvili2130_dep = declare_dependency(
    link_with: libdevili2130,
    include_directories: include_directories('.'),
)
