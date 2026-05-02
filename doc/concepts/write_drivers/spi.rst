Merlin-based SPI driver
-----------------------

.. index::
   single: SPI; driver
   single: include/merlin/buses/spi.h

The goal is to explain the software contract with Merlin (registration,
mapping, IRQ, bus API), not bit-level register programming details.

This section describes a sample SPI bus controller driver implementation that
follows the expected Merlin contract.

Note that this is a sample implementation, not a reference design. The goal is
to illustrate the expected driver structure and how it interacts with Merlin,
not to provide a production-ready driver for a specific SoC.

Driver metadata and fops
^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: spi_xfer_fn_t
   single: spi_bus_fops
   single: spi_driver
   single: DEVICE_TYPE_SPI

The Merlin entry point is ``include/merlin/buses/spi.h``.
This header defines the common contract an SPI bus driver must implement.
The SPI contract is intentionally minimal: SPI is a full-duplex bus, so the
single ``xfer`` callback covers both transmission and reception simultaneously.

- bus callback signature: ``xfer``;
- the ``struct spi_bus_fops`` table that groups this callback;
- the ``struct spi_driver`` descriptor that combines:
   - SPI ``fops``,
   - one ``platform_device_driver`` (generic Merlin side), including the ISR callback,
   - one ``private_data`` pointer (driver-specific state) if needed.

Required callback prototype (from ``spi.h``):

.. code-block:: c

   typedef int (*spi_xfer_fn_t)(struct spi_driver *self,
                                uint8_t *rdbuf,
                                uint8_t *wrbuf,
                                size_t len);

Callback usage:

- ``spi_xfer_fn_t``
   executes a full-duplex SPI transfer of ``len`` bytes. ``wrbuf`` holds the
   bytes to transmit; received bytes are written into ``rdbuf``. Either
   pointer may be ``NULL`` when only one direction is needed (e.g. read-only
   or write-only), but the driver must handle that case explicitly. The
   implementation drives the SPI controller according to the configured clock
   polarity, phase, and speed, and handles synchronisation (polling,
   interrupt, or DMA) internally.

The callback table is the bridge between Merlin's SPI contract and the driver
implementation:

.. code-block:: c

   static struct spi_bus_fops my_spi_specific_fops = {
       .xfer = my_driver_xfer,
   };

Required data structures (from ``spi.h``):

.. code-block:: c

   struct spi_bus_fops {
       spi_xfer_fn_t xfer;
   };

   struct spi_driver {
       struct spi_bus_fops *fops;
       struct platform_device *platform_fops;
       void *private_data;
   };

The platform-level descriptor is declared statically in the driver source and
references the probe, init, release and ISR callbacks through the embedded
``platform_fops`` table:

.. code-block:: c

   static struct platform_device_driver my_spi_driver = {
       .label      = MY_SPI_BUS_LABEL,
       .devh       = 0,               /* updated by probe */
       .name       = "my SPI driver for XXX",
       .compatible = "st,stm32",      /* adapt to actual SoC */
       .driver_fops = (void *)(&my_spi_specific_fops),
       .platform_fops = {
           .probe   = my_driver_probe,
           .init    = my_driver_init,
           .release = my_driver_release,
           .isr     = my_driver_isr,
       },
   };

This split is key:

- Merlin handles platform mechanics (DTS resolution, map/unmap, GPIO,
  IRQ routing),
- the driver handles SPI bus semantics (chip-select, clock polarity/phase,
  full-duplex data exchange).

In Merlin, ``struct spi_driver`` embeds ``struct platform_device_driver``.
This embedded structure is the generic runtime descriptor used by Merlin to:

- track device identity (``label``, ``compatible``, ``type``),
- store DTS-resolved metadata (``devinfo``),
- store driver registration state (``devh``),
- route interrupts through ``platform_fops.isr``.

Typical binding in an SPI driver instance:

.. code-block:: c

   drv->fops                       = &my_spi_specific_fops;
   drv->platform.name              = "my SPI driver";
   drv->platform.compatible        = "st,stm32";
   drv->platform.driver_fops       = (void *)(&my_spi_specific_fops);
   drv->platform.platform_fops.isr = my_driver_isr;

By setting ``platform_fops.isr``, the application can stay generic in its
event loop and call ``merlin_platform_driver_irq_displatch(IRQn)``. Merlin then
resolves the registered driver and invokes the SPI ISR callback.

Typical SPI driver pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: my_driver_probe()
   single: my_driver_init()
   single: my_driver_xfer()
   single: my_driver_release()
   single: my_driver_isr()
   single: merlin_platform_map()
   single: merlin_platform_unmap()
   single: merlin_platform_interrupt_iterate()

``my_driver_probe()`` retrieves the device handle from Merlin using the DTS
label and stores it in the platform descriptor. Merlin resolves all platform
resources (base address, IRQs, GPIO) at this point.

.. code-block:: c

   static int my_driver_probe(struct platform_device_driver *self,
                              uint32_t label)
   {
       self->devh = sys_get_handle(label);
       /* additional probe-time initialisation if needed */
       return 0;
   }

At this stage, the driver has a valid handle but the peripheral is not yet
accessible from the task. Mapping and hardware configuration are performed by
``my_driver_init()``.

``my_driver_init()`` maps the SPI controller into the task address space,
initialises the peripheral (clock divider, polarity, phase, data width), and
enables the relevant interrupts one by one using the Merlin interrupt iterator:

.. code-block:: c

   static int my_driver_init(struct platform_device_driver *self)
   {
       /* Map the SPI controller registers into the task address space */
       merlin_platform_map(self->devh);

       /* Initialise the SPI bus (clock, polarity, phase, data width…) */
       /* [...] */

       /* Enable all IRQs declared for this device in the DTS */
       for (uint32_t irq = merlin_platform_interrupt_iterate(self);
            irq != 0U;
            irq = merlin_platform_interrupt_iterate(self)) {
           sys_interrupt_enable(irq);
       }

       return 0;
   }

``my_driver_xfer()`` is the core callback. It executes one full-duplex
transaction of ``len`` bytes between the SPI controller and a slave device.
The implementation drives the controller (polling TXIS/RXNE flags, or via
DMA/interrupt depending on the design) and copies received data into ``rdbuf``
while consuming ``wrbuf``.

.. code-block:: c

   static int my_driver_xfer(struct platform_device_driver *self,
                              uint8_t *rdbuf,
                              uint8_t *wrbuf,
                              size_t len)
   {
       /* Drive the SPI transfer (polling / interrupt / DMA) */
       /* [...] */
       return 0;
   }

``my_driver_release()`` disables the SPI peripheral, masks all interrupts, and
unmaps the controller from the task address space:

.. code-block:: c

   static int my_driver_release(struct platform_device_driver *self)
   {
       /* Disable SPI bus, mask interrupts, etc. */
       /* [...] */

       /* Disable all IRQs declared for this device in the DTS */
       for (uint32_t irq = merlin_platform_interrupt_iterate(self);
            irq != 0U;
            irq = merlin_platform_interrupt_iterate(self)) {
           sys_interrupt_disable(irq);
       }

       /* Unmap the SPI controller registers */
       merlin_platform_unmap(self->devh);
       return 0;
   }

The ISR reacts to SPI hardware events (transfer complete, overrun, etc.) and
can stage received data or signal the upper layer:

.. code-block:: c

   static int my_driver_isr(struct platform_device_driver *self,
                             uint32_t IRQn)
   {
       /* React to the IRQ: read status register, clear flags, notify upper layer */
       /* [...] */
       return 0;
   }

Driver registration
^^^^^^^^^^^^^^^^^^^

.. index::
   single: merlin_platform_driver()
   single: SPI; registration

The driver descriptor is passed to Merlin via ``merlin_platform_driver()``.
This call is typically placed at task startup, before any SPI transaction is
requested. A macro wrapper may be provided in future Merlin versions to
automate this declaration:

.. code-block:: c

   /* Automatic declaration at task startup (macro TBD) */
   merlin_platform_driver(&my_spi_driver);

Once registered, ``my_driver_probe()`` is called by Merlin with the configured
DTS label, completing the initialisation sequence.

.. note::

   There is no need to declare each IRQ as owned by the device. The link
   between ``IRQn`` and ``devh_t`` is managed privately by Merlin using the
   DTS metadata resolved at probe time.
