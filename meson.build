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

with_doc_opt = get_option('with_doc')
if with_doc_opt
    subdir('doc')
endif

with_rust_opt = get_option('with_rust')
if with_rust_opt
    py3 = import('python').find_installation('python3', required: true)
    cargo = find_program('cargo', required: true)

    rust_dir = meson.current_source_dir() / 'rust'
    rust_target = 'thumbv8m.main-none-eabi'

    dts_include_dirs = []
    foreach d: get_option('dts-include-dirs')
        d = fs.expanduser(d)
        if not fs.is_absolute(d)
            d = meson.global_source_root() / d
        endif
        dts_include_dirs += [d]
    endforeach

    rust_env = environment()
    rust_env.set('MERLIN_DTS', dts_in.full_path())
    rust_env.set('MERLIN_DTS2SRC', dts2src.full_path())
    rust_env.set('MERLIN_DTS_INCLUDE_DIRS', ':'.join(dts_include_dirs))
    rust_env.set('MERLIN_CONFIG_TASK_LABEL', '0x0')

    rust_build_cmd = '''
import os
import pathlib
import subprocess
import sys

rust_dir, cargo, rust_target, stamp = sys.argv[1:5]
subprocess.check_call([cargo, "build", "--target", rust_target], cwd=rust_dir, env=os.environ.copy())
pathlib.Path(stamp).touch()
'''

    custom_target('rust-crate',
        output: 'rust-crate.stamp',
        input: [ dts_in ],
        command: [ py3, '-c', rust_build_cmd, rust_dir, cargo.full_path(), rust_target, '@OUTPUT@' ],
        env: rust_env,
        build_by_default: true,
        console: true,
    )
endif

summary(
    {
        'examples': with_examples_opt,
        'doc' : with_doc_opt,
        'rust' : with_rust_opt,
    },
    bool_yn: true,
    section: 'Targets',
)
