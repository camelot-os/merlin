Using Merlin in Drivers
=======================

.. index::
   single: Merlin; usage
   single: integration flow

This chapter describes the implementation flow used by the reference drivers in
``examples/`` and translates it into reusable guidance for new drivers.

Integration sequence
--------------------

.. index::
   single: probe
   single: init
   single: runtime
   single: release

For all bus classes, use the following sequence:

1. ``probe``
   Register one driver instance from its DTS label.
2. ``init``
   Map MMIO, configure pinmux/GPIO, and program controller defaults.
3. runtime API
   Execute transfers or endpoint operations with bounded polling and explicit
   state/error handling.
4. ``release``
   Disable peripheral behavior and unmap resources.

Calling runtime operations before probe/init should fail fast with
``DRV_ERROR_INVSTATE`` or ``DRV_ERROR_NOTREGISTERED``.

Bus controller pattern
----------------------

The sample bus drivers (I2C, SPI, USART, CAN, USB) share the same skeleton:

.. code-block:: c

   static struct platform_device_driver my_driver = {
       .label = 0,
       .compatible = "vendor,controller",
      .type = DEVICE_TYPE_SPI, /* or I2C/USART/CAN/USB */
       .platform_fops = {
           .isr = my_isr,
       },
   };

   drv_status_t my_probe(uint32_t label)
   {
       if (merlin_platform_driver_register(&my_driver, label) != STATUS_OK) {
           return DRV_ERROR_NOTREGISTERED;
       }
       return DRV_STATUS_OK;
   }

   drv_status_t my_init(...)
   {
       if (merlin_platform_driver_map(&my_driver) != STATUS_OK) {
           return DRV_ERROR_MAPFAILED;
       }
       if (merlin_platform_driver_configure_gpio(&my_driver) != STATUS_OK) {
           return DRV_ERROR_CONFIGURATION;
       }
       /* Controller-specific register programming */
       return DRV_STATUS_OK;
   }

   drv_status_t my_release(uint32_t label)
   {
       (void)label;
       if (merlin_platform_driver_unmap(&my_driver) != STATUS_OK) {
           return DRV_ERROR_MAPFAILED;
       }
       return DRV_STATUS_OK;
   }

Key points from the examples:

- register addresses are always derived from ``devinfo->baseaddr`` plus local
  offsets;
- MMIO is always done through ``merlin_ioread*``/``merlin_iowrite*``;
- wait loops are bounded through ``merlin_iopoll32_until_set/clear``;
- all error exits are explicit and mapped to ``drv_status_t``.

External device over a bus
--------------------------

The ILI2130 sample demonstrates the expected split between layers:

- bus driver responsibility:
  bus arbitration, START/STOP sequences, address mode handling, and controller
  register logic;
- external device driver responsibility:
  register map semantics (chip ID check, touch packet parsing, power/gesture
  programming), optionally plus dedicated GPIO behavior.

An external device driver can retrieve parent bus association from metadata and
use the unified bus API (for example ``i2c_read7()``/``i2c_write7()``) without
directly manipulating bus-controller registers.

Per-family notes from reference implementations
------------------------------------------------

I2C (``examples/i2c/i2c_bus_driver``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- validates addressing mode before each transfer path (7-bit vs 10-bit);
- handles NACK/BERR/ARLO/OVR explicitly;
- aborts in-flight transfer on failure and clears sticky flags;
- returns retry-oriented status for transient bus busy conditions.

SPI (``examples/spi/spi_bus_driver.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- uses ``spi_config`` as canonical input contract;
- derives prescaler from bus clock queried through
  ``merlin_platform_driver_get_bus_clock()``;
- supports software chip-select path through ``spi_set_cs()``;
- supports full-duplex and simplex transfer combinations with strict argument
  validation.

USART (``examples/usart/stm32_usart_driver.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- uses per-label instance slots to support multiple controllers;
- configures BRR from parent bus frequency via Merlin API;
- exposes both unified non-blocking API and optional blocking helper wrappers;
- ISR entry resolves instance from the ``platform_device_driver`` pointer passed
  by Merlin.

CAN (``examples/can/stm32u5_fdcan_driver.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- uses ``struct can_config`` and ``struct can_frame`` as stable contracts;
- resolves parent bus clock through
  ``merlin_platform_driver_get_bus_clock()`` for bitrate setup;
- separates init-mode controller programming from runtime TX/RX operations;
- returns retry-oriented status when TX FIFO/RX FIFO conditions are transient.

USB OTG FS (``examples/usb/usbotgfs_driver.c``)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

- keeps endpoint runtime state in dedicated tables;
- separates core reset/FIFO setup/interrupt mask configuration in independent
  helper stages;
- combines interrupt-driven completion with bounded polling helpers where
  hardware ordering requires it.

Application event loop integration
----------------------------------

Use one event loop and route IRQs through Merlin:

.. code-block:: c

   while (1) {
       /* wait_for_event(...) + event extraction */
       if (event.type == EVENT_TYPE_IRQ) {
           (void)merlin_platform_driver_irq_displatch(event.data[0]);
       }
   }

This keeps IRQ routing centralized and avoids duplicating controller-specific
IRQ lookup logic at application level.

Driver authoring checklist
--------------------------

Before considering a new driver production-ready, verify:

- DTS node exposes stable ``sentry,label`` and expected ``compatible``;
- driver ``type`` matches the intended Merlin backend;
- probe never performs MMIO before successful registration;
- init performs map + GPIO setup before enabling traffic/interrupts;
- runtime paths validate initialization state and parameters;
- release path disables active controller behavior then unmaps;
- exported public symbols match Merlin unified API names.
