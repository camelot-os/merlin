Device tree usage
-----------------

.. index::
  single: dts

Overview
"""""""""""""""""""""""""""""""""""""""""""""""

Merlin consumes DTS data at build time and exposes it to drivers through
``struct platform_device_driver`` at runtime.

The objective is to keep board-specific values (addresses, interrupts, pinmux,
ownership) in DTS and avoid hard-coding these values in driver C code.


Input artifacts
"""""""""""""""""""""""""""""""""""""""""""""""

The reference inputs in this repository are:

- ``dts/sample-i2c.dts``
- ``configs/sample.config``

The key linkage between the two files is:

- ``CONFIG_TASK_LABEL=0xC001F002`` in ``sample.config``
- ``sentry,owner = <0xC001F002>`` in ``sample-i2c.dts``

Only devices owned by the task are selected in generated Merlin metadata.


From DTS to generated C metadata
"""""""""""""""""""""""""""""""""""""""""""""""

For I2C devices, Merlin uses the template ``src/buses/i2c/i2c_dt.h.in``.
During build, this template is rendered into a generated header
(``builddir/src/buses/i2c/i2c_dt.h``) containing:

- ``i2c_devices[]``: one ``devinfo_t`` entry per active/owned I2C node,
- ``DEV_ID_I2C_MAX``: number of generated I2C devices,
- optional helper macros for pinctrl entries.

Each generated ``devinfo_t`` entry contains:

- ``id`` from ``sentry,label``,
- ``baseaddr`` and ``size`` from ``reg``,
- interrupt list from ``interrupts``,
- GPIO/pinmux descriptors from ``pinctrl-0``.


Runtime binding in Merlin
"""""""""""""""""""""""""""""""""""""""""""""""

At runtime, ``merlin_platform_driver_register()`` links driver code to DTS data:

1. select backend from ``driver->type`` (I2C path currently implemented),
2. resolve ``devinfo`` from the DTS-generated tables using the provided label,
3. request and store the kernel device handle,
4. persist the registered driver context for later IRQ dispatch.

After registration, the driver can call:

- ``merlin_platform_driver_map()`` / ``merlin_platform_driver_unmap()``
- ``merlin_platform_driver_configure_gpio()``
- ``merlin_platform_driver_irq_displatch()``

without manually re-parsing DTS data.


Concrete mapping from ``sample-i2c.dts``
"""""""""""""""""""""""""""""""""""""""""""""""

The sample defines two relevant nodes for the I2C example stack:

- ``&i2c1``

  - ``compatible = "st,stm32u5-i2c"``
  - ``sentry,label = <0x100>``
  - ``sentry,owner = <0xC001F002>``
  - ``pinctrl-0 = <&i2c1_scl_pb8>, <&i2c1_sda_pb9>``

- ``&touch``

  - ``compatible = "gpio,ili2130"``
  - ``sentry,label = <0x101>``
  - ``sentry,owner = <0xC001F002>``
  - ``pinctrl-0`` for reset and interrupt lines

In driver code, these labels are the only required runtime identifiers
(``i2c_driver_probe(0x100)``, then ``merlin_platform_driver_register(..., 0x101)``
for the touch controller).


Configuration requirements
"""""""""""""""""""""""""""""""""""""""""""""""

The sample configuration enables the Merlin runtime prerequisites:

- ``CONFIG_WITH_MERLIN=y``
- ``CONFIG_MAX_REGISTERED_DRIVERS=4``
- ``CONFIG_CAP_DEV_BUSES=y``
- ``CONFIG_CAP_DEV_IO=y``

These settings allow registration, MMIO mapping, and GPIO configuration through
Merlin APIs.

