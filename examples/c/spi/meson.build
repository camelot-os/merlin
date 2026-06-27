# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab Development Team

libdrvspi = static_library('drvspi',
    sources: [
        'spi_bus_driver.c',
    ],
    include_directories: include_directories('.'),
    dependencies: [ merlin_dep ],
)

drvi2c_dep = declare_dependency(
    link_with: libdrvspi,
)
