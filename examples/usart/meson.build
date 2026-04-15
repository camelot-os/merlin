# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2026 H2Lab Development Team

libdrvusart = static_library('drvusart',
    sources: [
        'stm32_usart_driver.c',
        'stm32_usart_driver.h',
    ],
    include_directories: include_directories('.'),
    dependencies: [ merlin_dep ],
)

drvusart_dep = declare_dependency(
    link_with: libdrvusart,
    include_directories: include_directories('.'),
)
