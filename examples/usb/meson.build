# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab Development Team

drvotgfs_hdr = files(
    'usbotgfs_driver.h',
)

libdrvstotgfs = static_library('drvstotgfs',
    sources: [
        'usbotgfs_driver.c',
        drvotgfs_hdr,
    ],
    include_directories: include_directories('.'),
    dependencies: [ merlin_dep ],
)


usbotgfs_dep = declare_dependency(
    link_with: libdrvstotgfs,
    include_directories: include_directories('.'),
)
