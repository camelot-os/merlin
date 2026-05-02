# SPDX-License-Identifier: Apache-2.0
# Copyright (c) 2026 H2Lab Development Team

if not with_doc_opt
    subdir_done()
endif

sphinx = find_program('sphinx-build')

pymod = import('python')
sphinx = find_program('sphinx-build')
doxygen = find_program('doxygen', required: false)

py3 = pymod.find_installation(
    'python3',
    modules: [
        'sphinx_rtd_theme',
        'sphinx_simplepdf',
    ]
)

concepts_img = files(
    'concepts/_static/figures/camelot.png',
    'concepts/_static/figures/merlin.png',
    'concepts/_static/figures/schema_block.png',
    'concepts/_static/figures/schema_sequence.png',
)

concepts_rst = files(
    'concepts/index.rst',
    'concepts/concepts/index.rst',
    'concepts/concepts/principles.rst',
    'concepts/concepts/usage.rst',
    'concepts/build_system/index.rst',
    'concepts/build_system/targets.rst',
    'concepts/tests/index.rst',
    'concepts/concepts/index.rst',
    'concepts/genindex.rst',
)

concepts = custom_target('doc-concepts',
    input: [ concepts_rst, concepts_img ],
    depend_files: [files('concepts/conf.py')],
    output: 'concepts',
    install: true,
    install_tag: 'doc',
    install_dir: get_option('datadir') / 'doc/merlin',
    command: [sphinx, '-b', 'html', '-q', '-d',
        'build/doctrees',
        '-Dversion=' + meson.project_version(),
        join_paths(meson.current_source_dir(), 'concepts'),
        'doc/concepts',
    ])
doc_concepts = alias_target('doc-concepts', concepts)
