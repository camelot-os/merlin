Merlin principles
=================

Layered API model
-----------------

Merlin exposes three complementary API layers:

- ``include/merlin/platform/driver.h``: platform abstraction for driver lifecycle
  (register, map, unmap, GPIO configuration, IRQ dispatch).
- ``include/merlin/buses/i2c.h``, ``include/merlin/buses/spi.h`` and
  ``include/merlin/buses/usb.h``: bus-specific operation contracts
  (I2C read/write callbacks, SPI transfer callback, USB endpoint callbacks).
- ``include/merlin/io.h``: architecture-agnostic MMIO primitives
  (8/16/32-bit read/write and polling helpers).

This split keeps the hardware-dependent code focused on register semantics, while
device discovery, ownership checks, and runtime integration stay in Merlin.


Driver identity and metadata
----------------------------

Each platform driver is described using ``struct platform_device_driver``.
Key fields are:

- ``label``: device identifier matching ``sentry,label`` in the DTS.
- ``compatible``: hardware compatibility string used by the driver logic.
- ``type``: family selector (I2C, SPI, USB, ...), used by Merlin backend routing.
- ``devinfo``: pointer to DTS-derived metadata (base address, size, IRQs, GPIO pinmux).
- ``platform_fops.isr``: ISR callback invoked through Merlin IRQ dispatching.

The objective is to keep all dynamic platform context in one object that is owned
by the driver.


Minimal lifecycle contract
--------------------------

For a bus controller or platform-visible device, the expected lifecycle is:

1. Register with ``merlin_platform_driver_register()`` using the DTS label.
2. Map with ``merlin_platform_driver_map()`` before touching MMIO.
3. Configure pinmux/GPIO through ``merlin_platform_driver_configure_gpio()`` when needed.
4. Run device operations (MMIO + protocol logic).
5. Optionally acknowledge interrupts through ``merlin_platform_acknowledge_irq()``.
6. Unmap with ``merlin_platform_driver_unmap()`` on release.

The I2C example implements this sequence explicitly in
``examples/i2c/i2c_bus_driver/i2c_bus_driver.c``.


I/O abstraction principle
-------------------------

The MMIO accessors from ``include/merlin/io.h`` hide architecture-specific assembly
through ``include/merlin/arch/asm-generic/io.h``.

As a result, driver code can stay architecture-independent and use:

- ``merlin_ioread8/16/32()`` and ``merlin_iowrite8/16/32()``
- ``merlin_iopoll32_until_set()`` and ``merlin_iopoll32_until_clear()``

This is the mechanism used by the I2C example to wait for controller flags
(``TXIS``, ``RXNE``, ``STOPF``, ``BUSY``) without embedding architecture-specific
instructions in the driver.


IRQ dispatch principle
----------------------

Merlin centralizes IRQ routing with ``merlin_platform_driver_irq_displatch()``:

- resolve the target driver from DTS-backed metadata,
- find the registered runtime driver context,
- call the driver ISR callback.

A task handling multiple devices can therefore keep one event loop and let Merlin
perform IRQ-to-driver routing.
