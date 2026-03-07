libi2c = static_library('i2c',
    sources: [
        'i2c_bus_driver.c',
    ],
    include_directories: include_directories('.'),
    dependencies: [ merlin_dep ],
)

i2c_dep = declare_dependency(
    link_with: libi2c,
    include_directories: include_directories('.'),
)
