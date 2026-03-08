# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab Development Team

merlin_common_src += files(
    'i2c.c',
)
merlin_private_headers += files(
    'i2c.h',
)

i2c_dt_h_template = meson.current_source_dir() / 'i2c_dt.h.in'

i2c_dt_h = custom_target('gen_i2c_dt_h',
    input: i2c_dt_h_template,
    env: {
        'CONFIG_TASK_LABEL': kconfig_data.get('CONFIG_TASK_LABEL', '0x0'),
    },
    output: '@BASENAME@',
    command: [ dts2src, '-d', dts.full_path() , '-t', '@INPUT@', '@OUTPUT@' ],
    depends: [ dts ],
)

generated_private_headers += [ i2c_dt_h ]
