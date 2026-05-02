Merlin-based I2C driver
-----------------------

.. index::
   single: I2C; driver
   single: include/merlin/buses/i2c.h

The goal is to explain the software contract with Merlin (registration,
mapping, IRQ, bus API), not bit-level register programming details.

This section describes a sample I2C bus controller driver implementation that
follows the expected Merlin contract.

Note that this is a sample implementation, not a reference design. The goal is
to illustrate the expected driver structure and how it interacts with Merlin,
not to provide a production-ready driver for a specific SoC.

Driver metadata and fops
^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: i2c_write_reg7_fn_t
   single: i2c_read_reg7_fn_t
   single: i2c_write_reg10_fn_t
   single: i2c_read_reg10_fn_t
   single: i2c_bus_fops
   single: i2c_driver
   single: i2c_speeds
   single: i2c_address_mode
   single: DEVICE_TYPE_I2C

The Merlin entry point is ``include/merlin/buses/i2c.h``.
This header defines the common contract an I2C bus driver must implement,
including at least the following set of standard I2C-related fields and
callbacks:

- speed enumeration: ``enum i2c_speeds`` (SM 100 kHz, FM 400 kHz, FM+ 1 MHz, HS 1.7/3.4 MHz, UFM 5 MHz);
- addressing mode enumeration: ``enum i2c_address_mode`` (``I2C_ADDRESS_7B``, ``I2C_ADDRESS_10B``);
- bus callback signatures: ``write7``, ``read7``, ``write10``, ``read10``;
- the ``struct i2c_bus_fops`` table that groups these callbacks;
- the ``struct i2c_driver`` descriptor that combines:
   - I2C ``fops``,
   - one ``platform_device_driver`` (generic Merlin side), including the ISR callback,
   - one ``private_data`` pointer (driver-specific state) if needed.

Required callback prototypes (from ``i2c.h``):

.. code-block:: c

   typedef int (*i2c_write_reg7_fn_t)(struct i2c_driver *self,
                                      uint8_t *wdbuf,
                                      size_t len);
   typedef int (*i2c_read_reg7_fn_t)(struct i2c_driver *self,
                                     uint8_t *rdbuf,
                                     size_t len);
   typedef int (*i2c_write_reg10_fn_t)(struct i2c_driver *self,
                                       uint8_t *wrbuf,
                                       size_t len);
   typedef int (*i2c_read_reg10_fn_t)(struct i2c_driver *self,
                                      uint8_t *rdbuf,
                                      size_t len);

Callback usage and driver-function mapping:

- ``i2c_write_reg7_fn_t``
   writes data to a slave using 7-bit addressing. Performs a complete I2C
   transaction (start, slave address, register address, data, stop). The
   buffer includes the register address followed by data bytes.
- ``i2c_read_reg7_fn_t``
   reads data from a slave using 7-bit addressing. Performs a write phase
   to set the register pointer, then a repeated-start read phase. Data is
   stored in the caller-supplied buffer.
- ``i2c_write_reg10_fn_t``
   same as ``write7`` but uses 10-bit addressing. The driver must check that
   the configured addressing mode matches before proceeding.
- ``i2c_read_reg10_fn_t``
   same as ``read7`` but uses 10-bit addressing.

The callback table is the bridge between Merlin's I2C contract and the
driver implementation:

.. code-block:: c

   static struct i2c_bus_fops g_i2c_fops = {
       .write7  = stm32_i2c_bus_write7,
       .read7   = stm32_i2c_bus_read7,
       .write10 = stm32_i2c_bus_write10,
       .read10  = stm32_i2c_bus_read10,
   };

Required data structures (from ``i2c.h``):

.. code-block:: c

   struct i2c_bus_fops {
       i2c_write_reg7_fn_t  write7;
       i2c_read_reg7_fn_t   read7;
       i2c_write_reg10_fn_t write10;
       i2c_read_reg10_fn_t  read10;
   };

   struct i2c_driver {
       struct i2c_bus_fops *fops;
       struct platform_device *platform_fops;
       void *private_data;
   };

The platform-level descriptor is declared statically in the driver and
partially filled at probe time by Merlin:

.. code-block:: c

   static struct platform_device_driver my_i2c_driver = {
       .label      = 0x0UL,           /* updated by probe */
       .devh       = 0,               /* updated by probe */
       .name       = "my I2C bus controller driver for STM32U5A5",
       .compatible = "st,stm32u5-i2c",
       .devinfo    = NULL,            /* updated by probe */
       .platform_fops = {
           .isr = stm32_i2c_isr,
       },
       .type = DEVICE_TYPE_I2C,
   };

This split is key:

- Merlin handles platform mechanics (DTS resolution, map/unmap, GPIO,
  IRQ routing),
- the driver handles I2C bus semantics (addressing mode, timing, read/write
  transactions).

In Merlin, ``struct i2c_driver`` embeds ``struct platform_device_driver``.
This embedded structure is the generic runtime descriptor used by Merlin to:

- track device identity (``label``, ``compatible``, ``type``),
- store DTS-resolved metadata (``devinfo``),
- store driver registration state (``devh``),
- route interrupts through ``platform_fops.isr``.

Typical binding in an I2C driver instance:

.. code-block:: c

   drv->fops                       = &g_i2c_fops;
   drv->platform.name              = "my I2C bus controller driver";
   drv->platform.compatible        = "st,stm32u5-i2c";
   drv->platform.driver_fops       = &g_i2c_fops;
   drv->platform.platform_fops.isr = stm32_i2c_isr;
   drv->platform.type              = DEVICE_TYPE_I2C;

By setting ``platform_fops.isr``, the application can stay generic in its
event loop and call ``merlin_platform_driver_irq_displatch(IRQn)``. Merlin then
resolves the registered driver and invokes the I2C ISR callback.

Typical I2C driver pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: i2c_driver_probe()
   single: i2c_driver_init()
   single: i2c_bus_write7()
   single: i2c_bus_read7()
   single: i2c_bus_write10()
   single: i2c_bus_read10()
   single: i2c_driver_release()

``i2c_driver_probe()`` registers the driver into Merlin using the DTS label
that uniquely identifies the I2C controller. All further metadata
(base address, IRQ numbers, GPIO pins) is resolved by Merlin from the DTS
at this point. The driver does not yet map or configure the peripheral.

.. code-block:: c

   int i2c_driver_probe(uint32_t label)
   {
       if (merlin_platform_driver_register(&my_i2c_driver, label) != STATUS_OK) {
           return -1;
       }
       return 0;
   }

At this stage, the driver is registered and associated with DTS metadata, but
it is not yet mapped or configured. Mapping and GPIO setup are done later by
``i2c_driver_init()``.

``i2c_driver_init()`` maps the controller into the task address space,
configures the SCL/SDA GPIO pins through Merlin, selects the addressing mode,
and programs the timing register for the requested bus speed.

.. code-block:: c

   int i2c_driver_init(enum i2c_speeds speed, enum i2c_address_mode mode)
   {
       /* Map the I2C controller in the application memory */
       if (merlin_platform_driver_map(&my_i2c_driver) != STATUS_OK) {
           return -1;
       }

       /* Configure GPIO pins for SCL and SDA */
       if (merlin_platform_driver_configure_gpio(&my_i2c_driver) != STATUS_OK) {
           return -1;
       }

       /* Set addressing mode (7-bit or 10-bit) */
       if (stm32_i2c_bus_set_addressing_mode(mode) != 0) {
           return -1;
       }

       /* Program timing register for the requested bus speed */
       stm32_i2c_bus_disable();
       stm32_i2c_bus_set_timing(timingr); /* value derived from speed */
       stm32_i2c_bus_enable();

       return 0;
   }

``i2c_bus_write7()`` performs a complete I2C write transaction in 7-bit
addressing mode. It waits for the bus to be idle, configures the transfer
parameters (slave address, byte count, auto-end), then sends the register
address byte followed by the data bytes one by one, polling the TXIS flag
before each write.

.. code-block:: c

   int i2c_bus_write7(uint8_t slave_addr, uint8_t reg_addr,
                      uint8_t *data, size_t length)
   {
       /* Wait for bus idle */
       if (stm32_i2c_bus_wait_until_idle() != 0) {
           return -1;
       }

       /* Configure CR2: slave addr, byte count, direction=write, autoend */
       if (stm32_i2c_bus_configure_transfer(slave_addr, length + 1U,
                                            0, 1, I2C_ADDRESS_7B) != 0) {
           return -1;
       }

       /* Send register address */
       if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0) {
           return -1;
       }
       merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), reg_addr);

       /* Send data bytes */
       for (size_t i = 0; i < length; i++) {
           if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0) {
               return -1;
           }
           merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), data[i]);
       }

       return stm32_i2c_bus_wait_for_stop();
   }

``i2c_bus_read7()`` performs a combined write-then-read transaction in 7-bit
addressing mode. A first write phase sends the register address; a
repeated-start read phase then clocks in the requested number of bytes by
polling the RXNE flag.

.. code-block:: c

   int i2c_bus_read7(uint8_t slave_addr, uint8_t reg_addr,
                     uint8_t *data, size_t length)
   {
       /* Write phase: send register address */
       if (stm32_i2c_bus_wait_until_idle() != 0 ||
           stm32_i2c_bus_configure_transfer(slave_addr, 1U, 0, 0,
                                            I2C_ADDRESS_7B) != 0) {
           return -1;
       }
       if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0) {
           return -1;
       }
       merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), reg_addr);

       /* Wait for transfer complete before repeated start */
       if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TC) != 0) {
           return -1;
       }

       /* Read phase: configure and receive data bytes */
       if (stm32_i2c_bus_configure_transfer(slave_addr, length, 1, 1,
                                            I2C_ADDRESS_7B) != 0) {
           return -1;
       }
       for (size_t i = 0; i < length; i++) {
           if (stm32_i2c_bus_wait_flag_set(I2C_ISR_RXNE) != 0) {
               return -1;
           }
           data[i] = merlin_ioread8(GET_REG_ADDR(I2C_RXDR_OFFSET));
       }

       return stm32_i2c_bus_wait_for_stop();
   }

``i2c_driver_release()`` disables the I2C peripheral, masks interrupts, and
unmaps the controller from the task address space through Merlin.

.. code-block:: c

   int i2c_driver_release(void)
   {
       stm32_i2c_bus_disable();
       stm32_i2c_disable_interrupt();

       if (merlin_platform_driver_unmap(&my_i2c_driver) != STATUS_OK) {
           return -1;
       }
       return 0;
   }

The ISR reads the I2C ISR register, clears the relevant flags (STOPF, NACKF,
BERR, ARLO, OVR), and can trigger upper-layer notifications. In a polling-only
design the ISR body can be left empty until event-driven behavior is required.

.. code-block:: c

   static void stm32_i2c_isr(struct i2c_driver *self, uint32_t IRQn)
   {
       uint32_t isr_val = merlin_ioread32(GET_REG_ADDR(I2C_ISR_OFFSET));
       uint32_t clear_flags = 0;

       if ((isr_val & I2C_ISR_STOPF) != 0UL) { clear_flags |= I2C_ICR_STOPCF; }
       if ((isr_val & I2C_ISR_NACKF) != 0UL) { clear_flags |= I2C_ICR_NACKCF; }
       if ((isr_val & I2C_ISR_BERR)  != 0UL) { clear_flags |= I2C_ICR_BERRCF; }
       if ((isr_val & I2C_ISR_ARLO)  != 0UL) { clear_flags |= I2C_ICR_ARLOCF; }
       if ((isr_val & I2C_ISR_OVR)   != 0UL) { clear_flags |= I2C_ICR_OVRCF;  }

       if (clear_flags != 0U) {
           stm32_i2c_bus_clear_flags(clear_flags);
       }
   }

Exported API aliases
^^^^^^^^^^^^^^^^^^^^

.. index::
   single: __attribute__((alias))
   single: i2c_driver; alias

The sample driver uses ``__attribute__((alias(...)))`` to decouple the public
API names from the internal implementation symbols, keeping the header and the
application code independent of the SoC vendor prefix:

.. code-block:: c

   int i2c_driver_probe(uint32_t label)
       __attribute__((alias("stm32_i2c_driver_probe")));
   int i2c_driver_init(enum i2c_speeds speed, enum i2c_address_mode mode)
       __attribute__((alias("stm32_i2c_driver_init")));
   int i2c_bus_write7(uint8_t slave_addr, uint8_t reg_addr,
                      uint8_t *data, size_t length)
       __attribute__((alias("stm32_i2c_bus_write7")));
   int i2c_bus_write10(uint16_t slave_addr, uint8_t reg_addr,
                       uint8_t *data, size_t length)
       __attribute__((alias("stm32_i2c_bus_write10")));
