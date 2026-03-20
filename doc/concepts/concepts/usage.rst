Using Merlin in a driver
========================

Typical integration flow
------------------------

The examples in ``examples/i2c`` illustrate a two-layer integration:

- a bus controller driver (STM32 I2C controller),
- a peripheral driver using this bus (ILI2130 touch controller).

The recommended startup sequence is:

1. Probe/register the bus driver from its DTS label.
2. Initialize and map the bus controller.
3. Probe and initialize the peripheral driver.
4. Enter the task event loop and dispatch interrupts through Merlin.


Bus driver pattern (I2C)
------------------------

In ``examples/i2c/i2c_bus_driver/i2c_bus_driver.c``, the bus driver:

- declares a ``platform_device_driver`` with ``.type = DEVICE_TYPE_I2C``,
- calls ``merlin_platform_driver_register(&my_i2c_driver, label)`` in probe,
- maps the device with ``merlin_platform_driver_map()`` in init,
- configures bus GPIOs with ``merlin_platform_driver_configure_gpio()``,
- accesses controller registers via ``merlin_ioread*`` / ``merlin_iowrite*``,
- unmaps on release with ``merlin_platform_driver_unmap()``.

The register base is never hard-coded in the driver logic; it is read from
``my_i2c_driver.devinfo->baseaddr`` (DTS-derived metadata).


Peripheral driver pattern (I2C device)
--------------------------------------

In ``examples/i2c/i2c_device_driver/ili2130_driver.c``, the peripheral driver:

- registers against Merlin using its own DTS label (``0x101`` in the sample),
- configures reset/interrupt GPIO pinmux through Merlin,
- communicates through the bus API (``i2c_bus_read7()``, ``i2c_bus_write7()``),
- keeps protocol-level logic (chip-ID validation, polling/touch parsing) in
  the device driver.

This split is the core Merlin usage model: bus mechanics stay in the bus driver,
functional behavior stays in the peripheral driver.


Application-level sequence example
----------------------------------

.. code-block:: c

   if (i2c_driver_probe(0x100) != 0) {
       return -1;
   }
   if (i2c_driver_init(I2C_SPEED_FM_400K, I2C_ADDRESS_7B) != 0) {
       return -1;
   }

   if (ilitech_2130_probe() != 0) {
       return -1;
   }
   if (ilitech_2130_init() != 0) {
       return -1;
   }

At runtime, when an interrupt event is received, dispatching is done through:

.. code-block:: c

   (void)merlin_platform_driver_irq_displatch(IRQn);

The matching driver ISR then acknowledges the interrupt with
``merlin_platform_acknowledge_irq()`` after processing device-specific status.


Implementation checklist
------------------------

For each new driver, verify the following points:

- DTS node has a stable ``sentry,label`` and matching ``compatible``.
- DTS node is owned by the task (``sentry,owner``).
- Driver ``type`` matches the Merlin backend used for registration.
- Probe performs registration before any map/MMIO access.
- Init configures required GPIO/pinmux before first transfer.
- Release unmaps the device and disables the active bus/IRQ sources.
