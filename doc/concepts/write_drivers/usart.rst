Merlin-based USART driver
-------------------------

.. index::
   single: USART; driver
   single: include/merlin/buses/usart.h


The goal is to explain the software contract with Merlin (registration,
mapping, IRQ, bus API), not bit-level register programming details.

This section describes a sample USART driver implementation that follows the
expected Merlin contract.

Note that this is a sample implementation, not a reference design. The goal is to
illustrate the expected driver structure and how it interacts with Merlin, not to
provide a production-ready driver for a specific SoC.

Driver metadata and fops
^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: usart_configure_fn_t
   single: usart_write_fn_t
   single: usart_read_fn_t
   single: usart_flush_fn_t
   single: usart_bus_fops
   single: usart_driver
   single: usart_config
   single: usart_operation_mode
   single: usart_parity
   single: usart_stop_bits
   single: usart_word_length
   single: usart_flow_control

The Merlin entry point is ``include/merlin/buses/usart.h``.
This header defines the common contract a USART driver must implement at least the following
set of standard USART-related fields and callbacks:

- line-configuration enums:  ``usart_operation_mode``, ``usart_parity``, ``usart_stop_bits``, ``usart_word_length``, ``usart_flow_control`` ;
- ``struct usart_config`` (baudrate, mode, parity, stop bits, word length, flow control, TX/RX enable);
- bus callback signatures: ``configure``, ``write``, ``read``, ``flush``;
- the ``struct usart_bus_fops`` table that groups these callbacks;
- the ``struct usart_driver`` descriptor that combines:
   - USART ``fops``,
   - one ``platform_device_driver`` (generic Merlin side), including the ISR callback,
   - one ``private_data`` pointer (driver-specific state) if needed, holding contextual content.

Required callback prototypes (from ``usart.h``):

.. code-block:: c

   typedef int (*usart_configure_fn_t)(struct usart_driver *self,
                                       const struct usart_config *config);
   typedef int (*usart_write_fn_t)(struct usart_driver *self,
                                   const uint8_t *wrbuf,
                                   size_t len);
   typedef int (*usart_read_fn_t)(struct usart_driver *self,
                                  uint8_t *rdbuf,
                                  size_t len);
   typedef int (*usart_flush_fn_t)(struct usart_driver *self);

Callback usage and driver-function mapping:

- ``usart_configure_fn_t``
   applies line settings (baudrate, parity,
   stop bits, word length, mode, flow control, TX/RX enable). In a driver,
   this is typically implemented by a function such as
   ``stm32_usart_fops_configure()`` and called during ``usart_init()``.
- ``usart_write_fn_t``
   pushes bytes to the TX path according to the
   controller policy (polling/interrupt/DMA). Typically implemented by
   ``stm32_usart_fops_write()`` and reached through ``usart_write()``.
- ``usart_read_fn_t``
   fetches bytes from the RX path. In interrupt-driven
   designs, this usually consumes data previously staged by the ISR in
   ``private_data``. Typically implemented by ``stm32_usart_fops_read()``
   and reached through ``usart_read()``.
- ``usart_flush_fn_t``
   ensures pending TX data is fully sent. Typically implemented by
   ``stm32_usart_fops_flush()`` and reached through ``usart_flush()``.

The callback table is the bridge between Merlin's USART contract and the
driver implementation:

.. code-block:: c

    static struct usart_bus_fops g_usart_fops = {
         .configure = stm32_usart_fops_configure,
         .write     = stm32_usart_fops_write,
         .read      = stm32_usart_fops_read,
         .flush     = stm32_usart_fops_flush,
    };

Required data structures (from ``usart.h``):

.. code-block:: c

   struct usart_config {
      uint32_t baudrate;
      enum usart_operation_mode mode;
      enum usart_parity parity;
      enum usart_stop_bits stop_bits;
      enum usart_word_length word_length;
      enum usart_flow_control flow_control;
      bool tx_enable;
      bool rx_enable;
   };

   struct usart_bus_fops {
      usart_configure_fn_t configure;
      usart_write_fn_t write;
      usart_read_fn_t read;
      usart_flush_fn_t flush;
   };

   struct usart_driver {
      struct usart_bus_fops *fops;
      struct platform_device_driver platform;
      void *private_data;
   };

For example, a usual driver implementation would hold:

1. a global ``g_usart_fops`` table defines USART callbacks;
2. each ``struct usart_driver`` instance embeds platform metadata
   (label DTS, ``compatible``, ``DEVICE_TYPE_USART``, ISR, etc.) ;
3. ``private_data`` holds lightweight RX state (received byte + flag).

This split is key:

- Merlin handles platform mechanics (DTS resolution, map/unmap, GPIO,
   IRQ routing),
- the driver handles USART bus semantics (configure/read/write/flush).

The same section also includes how platform metadata and ISR dispatch are
wired through the embedded platform driver descriptor.

In Merlin, ``struct usart_driver`` embeds ``struct platform_device_driver``.
This embedded structure is the generic runtime descriptor used by Merlin to:

- track device identity (``label``, ``compatible``, ``type``),
- store DTS-resolved metadata (``devinfo``),
- store driver registration state (``devh``),
- route interrupts through ``platform_fops.isr``.

Relevant generic platform definitions are:

.. code-block:: c

   typedef int (*merlin_platform_isr_fn_t)(void *self, uint32_t IRQn);

   struct platform_fops {
      merlin_platform_isr_fn_t isr;
   };

   struct platform_device_driver {
      devh_t devh;
      uint32_t label;
      const devinfo_t *devinfo;
      const char *name;
      const char *compatible;
      void *driver_fops;
      struct platform_fops platform_fops;
      device_type_t type;
   };

Typical binding in a USART driver instance:

.. code-block:: c

   drv->fops                       = &g_usart_fops;
   drv->platform.name              = "stm32 usart/uart driver";
   drv->platform.compatible        = "st,stm32-usart";
   drv->platform.driver_fops       = &g_usart_fops;
   drv->platform.platform_fops.isr = stm32_usart_isr;
   drv->platform.type              = DEVICE_TYPE_USART;

By setting ``platform_fops.isr``, the application can stay generic in its
event loop and call ``merlin_platform_driver_irq_displatch(IRQn)``. Merlin then
resolves the registered driver and invokes the USART ISR callback.

Typical USART driver pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: usart_probe()
   single: usart_init()
   single: usart_write()
   single: usart_read()
   single: usart_flush()
   single: usart_release()

``usart_probe()`` is responsible for creating/initializing one driver context,
binding the USART callbacks (whose signatures are defined by
``usart_configure_fn_t`` / ``usart_write_fn_t`` / ``usart_read_fn_t`` /
``usart_flush_fn_t``), and registering the driver into Merlin with the target
DTS label.

.. code-block:: c

   int usart_probe(uint32_t label)
   {
      struct usart_driver *drv = usart_instance_alloc();
      if (drv == NULL) {
         return -1;
      }

      /* 1) Bind USART contract callbacks to driver implementations */
      static struct usart_bus_fops g_usart_fops = {
         .configure = usart_fops_configure,
         .write     = usart_fops_write,
         .read      = usart_fops_read,
         .flush     = usart_fops_flush,
      };

      drv->fops                 = &g_usart_fops;
      drv->platform.driver_fops = &g_usart_fops;

      /* 2) Fill platform metadata used by Merlin */
      drv->platform.type              = DEVICE_TYPE_USART;
      drv->platform.compatible        = "vendor,soc-usart";
      drv->platform.platform_fops.isr = usart_isr;

      /* 3) Register against DTS label */
      if (merlin_platform_driver_register(&drv->platform, label) != STATUS_OK) {
         usart_instance_free(drv);   /* rollback local allocation */
         return -1;
      }

      return 0;
   }

At this stage, the driver is registered and associated with DTS metadata, but
it is not yet mapped or configured. Mapping and GPIO setup are done later by
``usart_init()``.

``usart_init()`` maps the device into the task address space, configures the
associated GPIOs (TX/RX and optional hardware flow-control pins) through
Merlin, then delegates line-parameter programming to the ``configure``
callback. IRQs are enabled at the end of ``configure`` once the peripheral
is fully programmed.

.. code-block:: c

   int usart_init(uint32_t label, const struct usart_config *cfg)
   {
      struct usart_driver *drv = usart_instance_get(label);
      if (drv == NULL || cfg == NULL) {
         return -1;
      }

      /* Make MMIO registers accessible in the task address space */
      if (merlin_platform_driver_map(&drv->platform) != STATUS_OK) {
         return -1;
      }

      /* Configure TX/RX (and optionally CTS/RTS) GPIO pins */
      if (merlin_platform_driver_configure_gpio(&drv->platform) != STATUS_OK) {
         return -1;
      }

      /* Apply line parameters and enable the peripheral + IRQs */
      return drv->fops->configure(drv, cfg);
      /* configure() programs baudrate, word length, parity, stop bits,
       * flow control, enables TX/RX, and calls
       * merlin_platform_driver_enable_irqs() before returning. */
   }

``usart_write()`` is a thin public wrapper that forwards one byte to the
``write`` callback. The callback waits for the TX data register to be empty
before placing the byte, ensuring back-pressure without explicit retry logic
in the application.

.. code-block:: c

   int usart_write(uint32_t label, uint8_t data)
   {
      struct usart_driver *drv = usart_instance_get(label);
      if (drv == NULL) {
         return -1;
      }

      /* Delegate to the fops write callback (polling TXE flag internally) */
      return drv->fops->write(drv, &data, 1);
   }

``usart_read()`` does not access hardware directly. RX is interrupt-driven:
the ISR captures the incoming byte into ``private_data`` and sets a flag.
``usart_read()`` checks that flag and consumes the staged byte atomically,
returning ``1`` (no data yet) or ``0`` (byte copied to caller buffer).

.. code-block:: c

   int usart_read(uint32_t label, uint8_t *rdbuf)
   {
      struct usart_driver *drv = usart_instance_get(label);
      if (drv == NULL || rdbuf == NULL) {
         return -1;
      }

      usart_private_t *priv = drv->private_data;

      if (priv->rxne_received) {
         rdbuf[0] = priv->rxne_data;  /* consume byte staged by ISR */
         priv->rxne_received = false;
         return 0;                    /* byte available */
      }

      return 1;                       /* no byte yet */
   }

``usart_release()`` disables the peripheral, unmaps it from the task address
space through Merlin, and frees the instance slot so it can be re-probed.

.. code-block:: c

   int usart_release(uint32_t label)
   {
      struct usart_driver *drv = usart_instance_get(label);
      if (drv == NULL) {
         return -1;
      }

      /* Disable the peripheral (clear UE bit in CR1) */
      usart_disable(drv);

      /* Unmap MMIO from the task address space */
      if (merlin_platform_driver_unmap(&drv->platform) != STATUS_OK) {
         return -1;
      }

      /* Release slot for future re-use */
      usart_instance_free(drv);
      return 0;
   }

Typical USART driver upper API
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: DEVICE_TYPE_USART
   single: usart_bus_fops; lifecycle

A usual USART driver would expose a set of public functions that implement the expected
USART contract (probe/init/write/read/flush/release) and delegate to the lower-level bus
callbacks. These functions are the high-level application-facing API keyed by a DTS ``label``.

Typical instance lifecycle:

1. ``usart_probe(label)`` allocates a driver slot, initializes
   ``struct usart_driver`` (fops + platform metadata), then registers the
   driver into Merlin through ``merlin_platform_driver_register``.
2. ``usart_init(label, &cfg)`` maps the device
   (``merlin_platform_driver_map``), configures GPIOs
   (``merlin_platform_driver_configure_gpio``), then applies
   ``struct usart_config`` through ``configure``.
3. ``usart_write(label, byte)`` is an application wrapper that delegates to
   ``write``.
4. ``usart_read(label, &byte)`` reads data already captured by the ISR and
   returns ``0`` when a byte is available, ``1`` otherwise.
5. ``usart_flush(label)`` waits for TX drain (``flush`` callback).
6. ``usart_release(label)`` disables the controller, unmaps it through
   Merlin, then frees the instance slot.

Practical note: in this example, RX is interrupt-driven.
The ISR updates ``private_data`` (byte + flag), and the public ``read`` API
consumes that state.

Example Camelot-OS application integration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: event loop
   single: merlin_platform_driver_irq_displatch()
   single: usart_config; example
   single: USART3_LABEL

In ``main.c``, the application follows the expected flow:

1. build a ``struct usart_config``;
2. call ``usart_probe()`` then ``usart_init()``;
3. enter an event loop;
4. for each IRQ received from the kernel, call
   ``merlin_platform_driver_irq_displatch(IRQn)`` ;
5. then call ``usart_read()`` to fetch the received byte.

Simplified snippet:

.. code-block:: c

    // USART label identifier, coherent with the DTS node label store in the sentry,label DTS field
    #define USART3_LABEL 0x103
    // [...]

    // Application level usart functional configuration
    struct usart_config my_config = {
       .baudrate = 115200,
       .mode = USART_MODE_ASYNCHRONOUS,
       .parity = USART_PARITY_NONE,
       .stop_bits = USART_STOP_BITS_1,
       .word_length = USART_WORD_LENGTH_8,
       .flow_control = USART_FLOW_CONTROL_NONE,
       .tx_enable = true,
       .rx_enable = true,
    };
    // [...]

   // probing against Merlin
   if (usart_probe(USART3_LABEL) != 0) {
      return -1;
   }
   // initialize the driver (mapping, GPIOs, line configuration, IRQs)
   if (usart_init(USART3_LABEL, &my_config) != 0) {
      return -1;
   }

   // Main event loop with single blocking point
   for (;;) {
      if (__sys_wait_for_event(EVENT_TYPE_IRQ, 0) != STATUS_OK) {
         __sys_sched_yield();
         continue;
      }

      copy_from_kernel(event_buf, SENTRY_SVCEXCHANGE_LEN);
      for (uint8_t i = 0; i < event->length / sizeof(uint32_t); i++) {
         (void)merlin_platform_driver_irq_displatch(IRQn[i]);
      }

      if (stm32_usart_read(USART3_LABEL, &rx_char) == 0) {
         /* process received byte */
      }
   }

This model cleanly separates responsibilities:

- the application orchestrates system events,
- Merlin routes IRQs to the right ISR,
- the USART driver implements the ``usart.h`` contract and provides standardized bus services.
