Basics and targets
------------------

.. index::
   single: build system
   single: Meson
   single: libmerlin.a

.. index::
  single: dependencies

Dependencies
""""""""""""

.. index::
  single: source hierarchy

About Merlin sources hierarchy
""""""""""""""""""""""""""""""

Merlin library
~~~~~~~~~~~~~~

The Merlin library is the main build target of the project. It is compiled as a
static library (``libmerlin.a``) from the sources described below.

Source layout
^^^^^^^^^^^^^

.. index::
   single: src/buses/
   single: src/platform/
   single: src/externals/
   single: include/merlin/buses
   single: include/merlin/platform
   single: include/merlin/io.h

Sources are organised under three subdirectories of ``src/``:

.. code-block:: none

   src/
   ├── buses/
   │   ├── i2c/       # I2C bus driver
   │   ├── usb/       # USB bus driver
   │   ├── usart/     # USART bus driver
   │   └── spi/       # SPI bus driver (reserved)
   ├── platform/      # hardware abstraction layer (HAL)
   └── externals/     # external device drivers

Public headers are exposed under ``include/merlin/``:

.. code-block:: none

   include/
   └── merlin/
       ├── buses/          # public bus APIs (i2c.h, usb.h, usart.h, spi.h)
       ├── platform/       # platform driver API (driver.h)
       ├── arch/           # register-access primitives, per architecture
       │   ├── asm-cortex-m/
       │   ├── asm-rv32/
       │   └── asm-generic/
       ├── io.h            # I/O primitives entry point
       └── helpers.h       # common utilities

Source integration
^^^^^^^^^^^^^^^^^^

.. index::
   single: merlin_sourceset
   single: ssmod.source_set()
   single: kconfig.h

The build system uses the Meson ``sourceset`` module to assemble sources
conditionally. Each subdirectory of ``src/`` appends its files to the
``merlin_common_src`` list, which is then wrapped in a ``source_set`` and
filtered against the active Kconfig symbols:

.. code-block:: meson

   merlin_sourceset = ssmod.source_set()
   # ... each subdirectory feeds merlin_common_src
   merlin_sourceset.add(merlin_common_src)
   merlin_sourceset_config = merlin_sourceset.apply(kconfig_data, strict: false).sources()

This allows drivers to be included or excluded automatically based on the active
Kconfig configuration, without any manual edits to the ``meson.build`` files.

DeviceTree-generated headers
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: dts2src
   single: i2c_dt.h
   single: usb_dt.h
   single: *_dt.h.in
   single: generated_private_headers

Each bus driver provides a template file ``*_dt.h.in`` that is processed by the
``dts2src`` tool against the compiled DeviceTree to produce a private C header.
These generated headers (e.g. ``i2c_dt.h``, ``usb_dt.h``) are tracked as
``generated_private_headers`` and added as a dependency of the library target:

.. code-block:: meson

   i2c_dt_h = custom_target('gen_i2c_dt_h',
       input:   i2c_dt_h_template,
       output:  '@BASENAME@',
       command: [ dts2src, '-d', dts.full_path(), '-t', '@INPUT@', '@OUTPUT@' ],
       depends: [ dts ],
   )
   generated_private_headers += [ i2c_dt_h ]

Final target
^^^^^^^^^^^^

.. index::
   single: merlin_dep
   single: declare_dependency()
   single: static_library()

The library is assembled in a ``static_library`` target that brings together:

* the sources selected by the Kconfig-filtered ``source_set``,
* ``kconfig.h``, force-included (``-include``) into every compilation unit,
* the public headers from ``include/``,
* the external ``shield`` and ``uapi`` dependencies.

A ``merlin_dep`` Meson dependency object is declared so that examples and
downstream projects can consume the library with a single ``dependency()`` call:

.. code-block:: meson

   merlin_dep = declare_dependency(
       link_with:           merlin_lib,
       dependencies:        [ external_deps ],
       include_directories: merlin_inc,
   )

Merlin examples
~~~~~~~~~~~~~~~

.. index::
   single: with_examples
   single: Merlin; examples

**Built by default**: false

.. code-block:: shell

   -Dwith_examples=false
