Merlin Principles
=================

.. index::
   single: Merlin
   single: Camelot-OS; userspace driver framework
   single: platform driver model

Design intent
-------------

Merlin implements a small, explicit contract between application code,
userspace drivers, and Camelot-OS platform services.

The framework design goals are:

- keep hardware-specific logic in drivers (register programming, protocol state);
- keep platform-specific integration in Merlin (DTS metadata, mapping,
  pinmux/GPIO setup, IRQ routing);
- provide a stable, family-oriented API so upper-layer code remains portable
  across SoCs and concrete driver implementations.

Compared to a monolithic BSP style, Merlin reduces duplication in probe/init
plumbing while preserving low-level control where it matters: MMIO sequencing,
timeouts, and hardware state machines.

Public interface map
--------------------

.. index::
   single: include/merlin/platform/driver.h
   single: include/merlin/platform/api/generic.h
   single: include/merlin/buses/i2c.h
   single: include/merlin/buses/spi.h
   single: include/merlin/buses/usart.h
   single: include/merlin/buses/usb.h
   single: include/merlin/io.h

The complete public API surface is exposed from ``include/merlin`` and can be
split into four layers:

.. list-table:: Merlin public API layers
   :header-rows: 1
   :widths: 25 35 40

   * - Header family
     - Purpose
     - Key symbols
   * - ``platform/driver.h``
     - Generic platform driver lifecycle and services
     - ``struct platform_device_driver``, ``device_type_t``,
       ``merlin_platform_driver_register()``,
       ``merlin_platform_driver_map()``,
       ``merlin_platform_driver_configure_gpio()``,
       ``merlin_platform_driver_irq_displatch()``
   * - ``platform/api/*.h``
     - Unified user-facing API per bus class
     - ``i2c_probe/init/read*/write*/release``,
       ``spi_probe/init/xfer/release/set_cs``,
       ``usart_probe/init/write/read/flush/release``
   * - ``buses/*.h``
     - Bus contracts and configuration enums/structures
     - ``struct spi_config``, ``struct usart_config``,
       I2C speed/addressing enums, USB endpoint operation callbacks
   * - ``io.h`` + ``helpers.h``
     - Architecture-neutral MMIO and polling helpers
     - ``merlin_ioread*()``, ``merlin_iowrite*()``,
       ``merlin_iopoll32_until_set()/until_clear()``, ``unlikely()``

Driver object model
-------------------

.. index::
   single: platform_device_driver
   single: devinfo_t
   single: sentry,label
   single: sentry,owner

Every platform-backed driver is anchored by ``struct platform_device_driver``.
At runtime, Merlin populates fields from generated DeviceTree metadata:

- ``label``: matches ``sentry,label`` and is the primary runtime selector;
- ``devinfo``: resolved metadata (base address, size, IRQs, pin data, and
  backend-specific attributes);
- ``devh``: kernel device handle bound at registration;
- ``platform_fops.isr``: ISR callback entry used by centralized IRQ dispatch;
- ``type``: backend discriminator (I2C, SPI, USART, USB, ...).

This keeps the platform context in one deterministic object owned by the
driver instance.

Lifecycle contract
------------------

.. index::
   single: probe
   single: init
   single: release
   single: lifecycle

Across I2C, SPI, USART, and USB sample drivers, the same lifecycle is used:

1. ``*_probe(label)``
   Register with Merlin and resolve DTS metadata ownership.
2. ``*_init(...)``
   Map MMIO, configure GPIO/pinctrl, initialize hardware state.
3. Runtime operations
   Execute transfers, endpoint I/O, or serial transactions.
4. ``*_release(label)``
   Disable hardware as needed and unmap resources.

The API is intentionally strict: runtime operations should reject calls when
probe/init has not completed or when labels do not match the registered
instance.

Error model
-----------

.. index::
   single: drv_status_t
   single: DRV_ERROR_TIMEOUT
   single: DRV_ERROR_AGAIN
   single: DRV_ERROR_CONFIGURATION

``include/merlin/platform/api/generic.h`` defines ``drv_status_t`` as a common
driver status space.

The sample drivers consistently use the following categories:

- state errors: ``DRV_ERROR_INVSTATE``, ``DRV_ERROR_NOTREGISTERED``;
- parameter errors: ``DRV_ERROR_INVPARAM``;
- platform/setup errors: ``DRV_ERROR_CONFIGURATION``, ``DRV_ERROR_MAPFAILED``;
- transfer/retry semantics: ``DRV_ERROR_TIMEOUT``, ``DRV_ERROR_AGAIN``;
- capability mismatches: ``DRV_ERROR_UNSUPPORTED_CFG``, ``DRV_ERROR_UNSUPPORTED``.

For robust upper layers, treat ``DRV_ERROR_AGAIN`` as a soft failure and most
other errors as hard failures requiring reconfiguration or reset.

MMIO and polling principles
---------------------------

.. index::
   single: merlin_ioread32()
   single: merlin_iowrite32()
   single: merlin_iopoll32_until_set()
   single: merlin_iopoll32_until_clear()

All sample bus drivers use ``include/merlin/io.h`` for register access.

Recommended pattern:

- compute register addresses from ``devinfo->baseaddr`` and local offsets;
- perform register reads/writes through ``merlin_ioread*`` and
  ``merlin_iowrite*`` only;
- gate hardware state transitions with bounded polls
  (``merlin_iopoll32_until_set/clear``) and return timeout errors when limits
  are reached.

This keeps code portable across supported architectures while preserving
deterministic behavior for bring-up and fault handling.

IRQ routing principle
---------------------

.. index::
   single: merlin_platform_driver_irq_displatch()
   single: platform_fops.isr
   single: event loop

Merlin centralizes IRQ-to-driver routing through
``merlin_platform_driver_irq_displatch(IRQn)``.

In an application event loop, the task forwards the IRQ number to Merlin;
Merlin then resolves the owning driver and calls the registered
``platform_fops.isr`` callback for that instance.

This allows one event loop to serve multiple drivers without duplicating
IRQ-number-to-device lookup logic in each application.

Portability rule from sample drivers
------------------------------------

The sample implementations export unified API symbols using aliasing, for
example:

.. code-block:: c

   drv_status_t i2c_probe(uint32_t label)
      __attribute__((alias("stm32_i2c_driver_probe")));

This pattern decouples application code from vendor-specific function names.
The same upper-layer binary contract can therefore target different concrete
drivers as long as they expose Merlin's public API symbols.
