Merlin-based USB driver
-----------------------

.. index::
   single: USB; driver
   single: include/merlin/buses/usb.h

The goal is to explain the software contract with Merlin (registration,
mapping, IRQ, bus API), not the full USB protocol stack details.

This section describes a sample USB OTG FS device-mode driver implementation
that follows the expected Merlin contract.

Note that this is a sample implementation, not a reference design. The goal is
to illustrate the expected driver structure and how it interacts with Merlin,
not to provide a production-ready USB driver for a specific SoC.

Driver metadata and fops
^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: usb_set_address_fn_t
   single: usb_write_endpoint_fn_t
   single: usb_read_endpoint_fn_t
   single: usb_bus_fops
   single: usb_driver
   single: usb_maximum_speed
   single: usb_otg_mode
   single: DEVICE_TYPE_USB

The Merlin entry point is ``include/merlin/buses/usb.h``.
This header defines the common contract a USB driver must implement, including
at least the following set of standard USB-related fields and callbacks:

- speed enumeration: ``enum usb_maximum_speed`` (``LOW``, ``FULL``, ``HIGH``, ``SUPER``);
- OTG mode enumeration: ``enum usb_otg_mode`` (``HOST``, ``DEVICE``);
- bus callback signatures: ``set_address``, ``write_endpoint``, ``read_endpoint``;
- the ``struct usb_bus_fops`` table that groups these callbacks;
- the ``struct usb_driver`` descriptor that combines:
   - USB ``fops``,
   - one ``platform_device_driver`` (generic Merlin side), including the ISR callback,
   - one ``private_data`` pointer (driver-specific state) if needed.

Required callback prototypes (from ``usb.h``):

.. code-block:: c

   typedef int (*usb_set_address_fn_t)(struct usb_driver *self,
                                       uint8_t address);
   typedef int (*usb_write_endpoint_fn_t)(struct usb_driver *self,
                                          uint8_t endpoint,
                                          const uint8_t *wrbuf,
                                          size_t len);
   typedef int (*usb_read_endpoint_fn_t)(struct usb_driver *self,
                                         uint8_t endpoint,
                                         uint8_t *rdbuf,
                                         size_t len);

Callback usage and driver-function mapping:

- ``usb_set_address_fn_t``
   programs the USB device address into the controller's DCFG register
   (DAD field) after the host sends a SET_ADDRESS request. Called by the
   USB control layer, not directly by the application.
- ``usb_write_endpoint_fn_t``
   queues an IN transfer on the specified endpoint. The driver programs the
   endpoint size register (DIEPTSIZ), writes the payload into the TX FIFO,
   then arms the endpoint (EPENA + CNAK). Completion is signalled by the
   ``XFRC`` interrupt on the corresponding IN endpoint.
- ``usb_read_endpoint_fn_t``
   registers a receive buffer for an OUT endpoint. The driver stores the
   destination pointer and expected byte count; the ISR fills the buffer when
   data arrives in the RX FIFO and notifies the upper layer through the
   registered endpoint handler.

The callback table is the bridge between Merlin's USB contract and the driver
implementation:

.. code-block:: c

   static struct usb_bus_fops g_usb_fops = {
       .set_address    = usbotgfs_set_address,
       .write_endpoint = usbotgfs_write_endpoint,
       .read_endpoint  = usbotgfs_read_endpoint,
   };

Required data structures (from ``usb.h``):

.. code-block:: c

   struct usb_bus_fops {
       usb_set_address_fn_t    set_address;
       usb_write_endpoint_fn_t write_endpoint;
       usb_read_endpoint_fn_t  read_endpoint;
   };

   struct usb_driver {
       struct usb_bus_fops *fops;
       struct platform_device *platform_fops;
       void *private_data;
   };

The platform-level descriptor is declared statically in the driver and filled
at probe time by Merlin:

.. code-block:: c

   static struct platform_device_driver g_usbotgfs_driver = {
       .devh       = 0,
       .label      = 0,
       .devinfo    = NULL,
       .name       = "stm32 usbotgfs minimal driver",
       .compatible = "st,stm32-otgfs",
       .driver_fops = NULL,
       .platform_fops = {
           .isr = usbotgfs_isr,
       },
       .type = DEVICE_TYPE_USB,
   };

This split is key:

- Merlin handles platform mechanics (DTS resolution, map/unmap, GPIO,
  IRQ routing),
- the driver handles USB bus semantics (FIFO management, endpoint
  configuration, device-mode bring-up, ISR dispatch).

In Merlin, ``struct usb_driver`` embeds ``struct platform_device_driver``.
This embedded structure is the generic runtime descriptor used by Merlin to:

- track device identity (``label``, ``compatible``, ``type``),
- store DTS-resolved metadata (``devinfo``),
- store driver registration state (``devh``),
- route interrupts through ``platform_fops.isr``.

Typical binding in a USB driver instance:

.. code-block:: c

   drv->fops                       = &g_usb_fops;
   drv->platform.name              = "stm32 usbotgfs minimal driver";
   drv->platform.compatible        = "st,stm32-otgfs";
   drv->platform.driver_fops       = NULL; /* handled through fops */
   drv->platform.platform_fops.isr = usbotgfs_isr;
   drv->platform.type              = DEVICE_TYPE_USB;

By setting ``platform_fops.isr``, the application can stay generic in its
event loop and call ``merlin_platform_driver_irq_displatch(IRQn)``. Merlin then
resolves the registered driver and invokes the USB ISR callback.

Typical USB driver pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: usbotgfs_probe()
   single: usbotgfs_init()
   single: usbotgfs_send_data()
   single: usbotgfs_set_recv_fifo()
   single: usbotgfs_isr()
   single: USB_OTG_MODE_DEVICE

``usbotgfs_probe()`` registers the driver descriptor into Merlin. Merlin
resolves the DTS node matching the ``compatible`` string and the supplied
label, stores the device handle and base address in the descriptor, and
returns.

.. code-block:: c

   int usbotgfs_probe(uint32_t label)
   {
       if (merlin_platform_driver_register(&g_usbotgfs_driver, label)
               != STATUS_OK) {
           return -1;
       }
       return 0;
   }

At this stage, the driver is registered and associated with DTS metadata, but
it is not yet mapped or configured. Mapping and hardware bring-up are done
later by ``usbotgfs_init()``.

``usbotgfs_init()`` maps the controller into the task address space, configures
the USB GPIO pins, resets the OTG core, sizes the RX/TX FIFOs, sets device
speed, arms EP0 for SETUP reception, and enables the global interrupt mask.

.. code-block:: c

   int usbotgfs_init(enum usb_otg_mode mode,
                     enum usb_maximum_speed max_speed)
   {
       if (mode != USB_OTG_MODE_DEVICE) {
           return -1;
       }

       /* Map MMIO registers and configure D+/D- GPIO pins */
       if (merlin_platform_driver_map(&g_usbotgfs_driver) != STATUS_OK) {
           return -1;
       }
       if (merlin_platform_driver_configure_gpio(&g_usbotgfs_driver)
               != STATUS_OK) {
           return -1;
       }

       /* Core reset, FS device-mode selection, FIFO sizing,
        * EP0 baseline setup, interrupt mask configuration */
       return usbotgfs_core_device_init();
   }

``usbotgfs_send_data()`` queues an IN transfer on a given endpoint. It
computes packet count, arms the DIEPTSIZ register, verifies TX FIFO space,
writes the payload word-by-word via ``usbotgfs_fifo_write()``, then sets
EPENA + CNAK in the DIEPCTL register to start the transfer.

.. code-block:: c

   int usbotgfs_send_data(uint8_t *src, uint32_t size, uint8_t ep)
   {
       uint16_t mps = usbotgfs_ep_get_mps(ep, USBOTG_FS_EP_DIR_IN);
       uint32_t pktcnt = (size + mps - 1U) / mps;

       /* Program transfer size register */
       usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPTSIZ_OFFSET),
                        size | (pktcnt << USB_DXEPTSIZ_PKTCNT_SHIFT));

       /* Write payload into the endpoint TX FIFO */
       usbotgfs_fifo_write(ep, src, size);

       /* Arm the endpoint: clear NAK and enable */
       uint32_t ctl = usbotgfs_read32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET));
       ctl |= USB_DXEPCTL_CNAK | USB_DXEPCTL_EPENA;
       usbotgfs_write32(usbotgfs_diep_offset(ep, USB_DIEPCTL_OFFSET), ctl);

       return 0;
   }

``usbotgfs_set_recv_fifo()`` registers a receive buffer and arms an OUT
endpoint for reception. The driver stores the destination pointer and expected
byte count; actual data is deposited by the ISR when RXFLVL fires.

.. code-block:: c

   int usbotgfs_set_recv_fifo(uint8_t *dst, uint32_t size, uint8_t ep)
   {
       /* Store destination and size for ISR-driven fill */
       g_ep_rx[ep].dst      = dst;
       g_ep_rx[ep].expected = size;
       g_ep_rx[ep].received = 0U;

       /* Arm DOEPTSIZ and set EPENA + CNAK */
       /* [...] */
       return 0;
   }

The ISR is the central event dispatcher for the USB OTG FS core. It reads
GINTSTS masked by GINTMSK, handles each active interrupt flag, and clears
processed bits before returning:

.. code-block:: c

   static int usbotgfs_isr(void *self, uint32_t IRQn)
   {
       uint32_t gintsts = usbotgfs_read32(USB_GINTSTS_OFFSET)
                        & usbotgfs_read32(USB_GINTMSK_OFFSET);
       uint32_t clear_mask = 0UL;

       if (gintsts & USB_GINTSTS_USBRST) {
           usbotgfs_reset_runtime_state();
           usbotgfs_arm_ep0_setup_reception();
           usbctrl_handle_reset(g_usbotgfs_driver.label);
           clear_mask |= USB_GINTSTS_USBRST;
       }
       if (gintsts & USB_GINTSTS_ENUMDNE) {
           usbotgfs_update_port_speed();
           clear_mask |= USB_GINTSTS_ENUMDNE;
       }
       if (gintsts & USB_GINTSTS_RXFLVL) {
           usbotgfs_handle_rxflvl(); /* drains RX FIFO into ep buffers */
       }
       if (gintsts & USB_GINTSTS_IEPINT) {
           usbotgfs_handle_iepint(); /* IN transfer complete */
           clear_mask |= USB_GINTSTS_IEPINT;
       }
       if (gintsts & USB_GINTSTS_OEPINT) {
           usbotgfs_handle_oepint(); /* OUT/SETUP received */
           clear_mask |= USB_GINTSTS_OEPINT;
       }
       if (gintsts & USB_GINTSTS_USBSUSP) {
           usbctrl_handle_usbsuspend(g_usbotgfs_driver.label);
           clear_mask |= USB_GINTSTS_USBSUSP;
       }
       if (gintsts & USB_GINTSTS_WKUINT) {
           usbctrl_handle_wakeup(g_usbotgfs_driver.label);
           clear_mask |= USB_GINTSTS_WKUINT;
       }

       usbotgfs_write32(USB_GINTSTS_OFFSET, clear_mask);
       return 0;
   }

Endpoint handler registration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: usbotgfs_ioep_handler_t
   single: USB; endpoint handler
   single: usbctrl_handle_reset()
   single: usbctrl_handle_inepevent()
   single: usbctrl_handle_outepevent()

Upper-layer USB stacks (e.g. a USB control library) register per-endpoint
callbacks of type ``usbotgfs_ioep_handler_t``. The ISR invokes these handlers
when a transfer completes, passing the device handle, received byte count,
and endpoint number:

.. code-block:: c

   typedef int (*usbotgfs_ioep_handler_t)(devh_t devh,
                                          size_t size,
                                          uint8_t ep);

This mechanism decouples the low-level OTG FS driver from the USB protocol
stack: the driver delivers raw transfer events; the stack interprets SETUP
packets, manages descriptor enumeration, and drives class-specific behaviour.

FIFO memory map
^^^^^^^^^^^^^^^

.. index::
   single: USB; FIFO
   single: GRXFSIZ
   single: GNPTXFSIZ
   single: DIEPTXF

The OTG FS core on STM32 provides 320 words (1280 bytes) of shared FIFO
memory. The sample driver partitions this memory as follows:

- **RX FIFO**: 128 words — shared across all OUT endpoints and SETUP packets;
- **Non-periodic TX FIFO (EP0)**: 64 words — used for control IN transfers;
- **Per-endpoint TX FIFOs (EP1–EP7)**: sized individually (32/32/32/8/8/8/8 words).

These values are programmed into GRXFSIZ, GNPTXFSIZ, and the DIEPTXF registers
during ``usbotgfs_init()``.
