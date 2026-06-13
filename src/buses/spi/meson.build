# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab OSS Team

merlin_common_src += files(
	'spi.c',
)
merlin_private_headers += files(
	'spi.h',
)

spi_dt_h_template = meson.current_source_dir() / 'spi_dt.h.in'

spi_dt_h = custom_target('gen_spi_dt_h',
	input: spi_dt_h_template,
	env: {
		'CONFIG_TASK_LABEL': kconfig_data.get('CONFIG_TASK_LABEL', '0x0'),
	},
	output: '@BASENAME@',
	command: [ dts2src, '-d', dts.full_path() , '-t', '@INPUT@', '@OUTPUT@' ],
	depends: [ dts ],
)

generated_private_headers += [ spi_dt_h ]
