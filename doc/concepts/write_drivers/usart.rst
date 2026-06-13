Merlin-based USART driver
-------------------------

.. index::
   single: USART; driver
   single: include/merlin/platform/api/usart.h
   single: drv_status_t

This section documents the expected software contract for UART/USART
controllers integrated in Merlin.

Public API contract
^^^^^^^^^^^^^^^^^^^

The canonical exported API is defined in
``include/merlin/platform/api/usart.h``:

- ``usart_probe(uint32_t label)``
- ``usart_init(uint32_t label, const struct usart_config *cfg)``
- ``usart_write(uint32_t label, uint8_t data)``
- ``usart_read(uint32_t label, uint8_t *data)``
- ``usart_flush(uint32_t label)``
- ``usart_release(uint32_t label)``

Applications call these functions directly. No bus-level callback table is
required for the public path.

USART line configuration types are declared in
``include/merlin/buses/usart.h`` (``struct usart_config`` and related enums).

Status model
^^^^^^^^^^^^

All exported USART symbols return ``drv_status_t``.

Recommended conventions:

- not registered or invalid label: ``DRV_ERROR_NOTREGISTERED``
- invalid lifecycle (init before probe, release before init): ``DRV_ERROR_INVSTATE``
- invalid argument (NULL cfg/data, unsupported value): ``DRV_ERROR_INVPARAM``
- temporary unavailable RX/TX path (non-blocking operation): ``DRV_ERROR_AGAIN``
- flush timeout: ``DRV_ERROR_TIMEOUT``
- pinmux/clock/MMIO configuration failure: ``DRV_ERROR_CONFIGURATION``

Driver structure and platform integration
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``include/merlin/buses/usart.h`` provides a lightweight runtime container:

.. code-block:: c

   struct usart_driver {
      struct platform_device_driver platform;
      void *private_data;
   };

Only ``platform_fops.isr`` is used in ``platform_device_driver`` for interrupt
dispatch.

.. code-block:: c

   drv->platform.name              = "stm32 usart/uart driver";
   drv->platform.compatible        = "st,stm32-usart";
   drv->platform.platform_fops.isr = stm32_usart_isr;
   drv->platform.type              = DEVICE_TYPE_USART;

Probe/init/runtime/release pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   drv_status_t usart_probe(uint32_t label)
   {
      if (merlin_platform_driver_register(&my_usart.platform, label) != STATUS_OK) {
         return DRV_ERROR_NOTREGISTERED;
      }
      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usart_init(uint32_t label, const struct usart_config *cfg)
   {
      (void)label;

      if (cfg == NULL) {
         return DRV_ERROR_INVPARAM;
      }

      if (merlin_platform_driver_map(&my_usart.platform) != STATUS_OK) {
         return DRV_ERROR_CONFIGURATION;
      }

      if (merlin_platform_driver_configure_gpio(&my_usart.platform) != STATUS_OK) {
         return DRV_ERROR_CONFIGURATION;
      }

      if (my_usart_configure(cfg) != 0) {
         return DRV_ERROR_INVPARAM;
      }

      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usart_write(uint32_t label, const uint8_t data)
   {
      (void)label;

      if (!my_usart_tx_ready()) {
         return DRV_ERROR_AGAIN;
      }

      my_usart_tx_push(data);
      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usart_read(uint32_t label, uint8_t *data)
   {
      (void)label;

      if (data == NULL) {
         return DRV_ERROR_INVPARAM;
      }

      if (!my_usart_rx_ready()) {
         return DRV_ERROR_AGAIN;
      }

      *data = my_usart_rx_pop();
      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usart_flush(uint32_t label)
   {
      (void)label;

      if (my_usart_wait_tc() != 0) {
         return DRV_ERROR_TIMEOUT;
      }

      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t usart_release(uint32_t label)
   {
      (void)label;

      if (merlin_platform_driver_unmap(&my_usart.platform) != STATUS_OK) {
         return DRV_ERROR_CONFIGURATION;
      }

      return DRV_STATUS_OK;
   }

Alias pattern (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   drv_status_t usart_probe(uint32_t label)
      __attribute__((alias("stm32_usart_probe")));
   drv_status_t usart_init(uint32_t label, const struct usart_config *cfg)
      __attribute__((alias("stm32_usart_init")));
   drv_status_t usart_write(uint32_t label, const uint8_t data)
      __attribute__((alias("stm32_usart_write")));
   drv_status_t usart_read(uint32_t label, uint8_t *data)
      __attribute__((alias("stm32_usart_read")));
   drv_status_t usart_flush(uint32_t label)
      __attribute__((alias("stm32_usart_flush")));
   drv_status_t usart_release(uint32_t label)
      __attribute__((alias("stm32_usart_release")));
