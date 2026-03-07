# SPDX-FileCopyrightText: 2023 - 2025 H2Lab OSS Team
# SPDX-License-Identifier: Apache-2.0 OR BSD-3-Clause

project('merlin', 'c',
    meson_version: '>=1.7.0',
    version : run_command('support/meson/version.sh', 'get-vcs', check: true).stdout().strip(),
    default_options: [
        'c_std=gnu11',
        'warning_level=3', 'optimization=s',
        'default_library=static', 'b_staticpic=false',
        'licensedir=share/info/libmerlin',
        'pkgconfig.relocatable=true',
        'config=configs/defconfig',
])

pkg = import('pkgconfig')
fs = import('fs')
ssmod = import('sourceset')

kconfig_file = meson.current_source_dir() / 'Kconfig'
kconfig_proj = subproject('kconfig', default_options: ['kconfig=@0@'.format(kconfig_file)])
kconfig_h = kconfig_proj.get_variable('kconfig_h')
kconfig_data = kconfig_proj.get_variable('kconfig_data')

devicetree_proj = subproject('devicetree', default_options:['install=true'])
dts_in = devicetree_proj.get_variable('devicetree_dts_in')
dts = devicetree_proj.get_variable('devicetree_dtsd')
dts2src = devicetree_proj.get_variable('dts2src')
dtsgen = generator(dts2src,
    arguments: ['-d', dts.full_path() , '-t', '@INPUT@', '@OUTPUT@' ],
    depends: [ dts ],
    output: '@BASENAME@',
)


global_build_args = [
    '-include', fs.parent(kconfig_h) / fs.name(kconfig_h),
]
external_deps = []
generated_private_headers = []

#if meson.is_cross_build()
#    libshield_proj = subproject('shield')
shield_dep = dependency('shield', required: true)
uapi_dep = dependency('uapi', required: true)
    #libshield_dep = libshield_proj.get_variable('shield_c_dep')
external_deps += [ shield_dep, uapi_dep ]
#endif

merlin_inc = include_directories('include')
merlin_sourceset = ssmod.source_set()
subdir('include')
subdir('src')

#with_tests_opt = get_option('with_tests')
#if with_tests_opt
#    subdir('tests')
#endif

# for all generated headers generated in the src directory, we need to add them
# as dependencies to the library target, otherwise the build system won't know about them
# and won't trigger the generation when needed.
merlin_private_headers_dep = declare_dependency(
    sources: generated_private_headers,
)

merlin_lib = static_library(meson.project_name(),
    sources: merlin_sourceset_config,
    c_args: [ global_build_args ],
    include_directories: [ merlin_inc ],
    dependencies: [ external_deps, merlin_private_headers_dep ],
)

merlin_dep = declare_dependency(
    link_with: merlin_lib,
    dependencies: [ external_deps ],
    include_directories: merlin_inc,
)

# about documentation building
with_examples_opt = get_option('with_examples')
if with_examples_opt
    subdir('examples')
endif
