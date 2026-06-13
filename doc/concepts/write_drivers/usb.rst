Merlin-based USB driver
-----------------------

.. index::
   single: USB; driver
   single: include/merlin/buses/usb.h
   single: drv_status_t

This section documents how to integrate a USB controller driver in Merlin with
the current public architecture.

Current API status
^^^^^^^^^^^^^^^^^^

Unlike I2C/SPI/USART, USB does not yet expose a dedicated
``include/merlin/platform/api/usb.h`` unified API header.

At this stage:

- USB bus-level enums are defined in ``include/merlin/buses/usb.h``
  (``enum usb_maximum_speed`` and ``enum usb_otg_mode``);
- concrete function sets are implementation-specific (for example
  ``examples/usb/usbotgfs_driver.h``);
- legacy callback-table documentation is obsolete and should not be used as a
   contract reference.

Recommended transition model
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

To stay consistent with the rest of Merlin while USB API unification is still
in progress, new or updated USB drivers should follow these rules:

- keep low-level implementation symbols driver-specific (vendor/controller
  prefixed);
- expose stable entry points expected by the upper layer;
- prefer ``drv_status_t`` for newly introduced public-facing wrappers;
- convert raw HW failures into Merlin generic status codes instead of ``0/-1``.

Status mapping guidelines
^^^^^^^^^^^^^^^^^^^^^^^^^

For public wrapper functions, map errors using ``drv_status_t``:

- invalid endpoint or null buffer: ``DRV_ERROR_INVPARAM``
- not initialized / invalid sequence: ``DRV_ERROR_INVSTATE``
- endpoint busy / retryable TX-RX state: ``DRV_ERROR_AGAIN``
- timeout waiting for transfer completion: ``DRV_ERROR_TIMEOUT``
- unsupported speed/mode/endpoint type: ``DRV_ERROR_UNSUPPORTED_CFG``
- successful operation: ``DRV_STATUS_OK``

Platform integration pattern
^^^^^^^^^^^^^^^^^^^^^^^^^^^^

USB controller drivers still use ``struct platform_device_driver`` for:

- registration from DTS label,
- MMIO map/unmap,
- IRQ handler registration.

Pseudo-code for lifecycle wrappers:

.. code-block:: c

   drv_status_t usb_probe(uint32_t label)
   {
      if (merlin_platform_driver_register(&my_usb_driver, label) != STATUS_OK) {
         return DRV_ERROR_NOTREGISTERED;
      }
      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usb_init(uint32_t label,
                         enum usb_otg_mode mode,
                         enum usb_maximum_speed speed)
   {
      (void)label;

      if (merlin_platform_driver_map(&my_usb_driver) != STATUS_OK) {
         return DRV_ERROR_MAPFAILED;
      }

      if (my_usb_configure(mode, speed) != 0) {
         return DRV_ERROR_UNSUPPORTED_CFG;
      }

      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usb_release(uint32_t label)
   {
      (void)label;

      if (merlin_platform_driver_unmap(&my_usb_driver) != STATUS_OK) {
         return DRV_ERROR_CONFIGURATION;
      }

      return DRV_STATUS_OK;
   }

Note on existing examples
^^^^^^^^^^^^^^^^^^^^^^^^^

Current USB examples may still expose ``int`` return values for low-level
controller helpers.

This is acceptable for internal/private functions. For future public API
alignment, prefer adding ``drv_status_t`` wrapper functions around these helper
calls so upper layers can rely on the same status model as other bus drivers.
