Merlin-based I2C driver
-----------------------

.. index::
   single: I2C; driver
   single: include/merlin/platform/api/i2c.h
   single: drv_status_t

This section explains the software contract expected by Merlin for an I2C bus
controller driver.

The goal is to document the integration model (probe/init/runtime/release),
not the bit-level programming details of a specific I2C controller.

Public API contract
^^^^^^^^^^^^^^^^^^^

The application-facing contract is declared in
``include/merlin/platform/api/i2c.h``.

The bus header ``include/merlin/buses/i2c.h`` now only provides I2C types
(speed and address mode enums). Legacy callback typedefs and callback tables
are not part of the public contract anymore.

Expected exported symbols are:

- ``i2c_probe(uint32_t label)``
- ``i2c_init(uint32_t label, enum i2c_speeds speed, enum i2c_address_mode mode)``
- ``i2c_write7(...)`` / ``i2c_read7(...)``
- ``i2c_write10(...)`` / ``i2c_read10(...)``
- ``i2c_release(uint32_t label)``

All these functions return ``drv_status_t``.

Status model
^^^^^^^^^^^^

``drv_status_t`` is defined in ``include/merlin/platform/api/generic.h`` and
must be used by exported I2C symbols.

Typical mapping in a bus driver:

- registration failure: ``DRV_ERROR_NOTREGISTERED``
- invalid parameters: ``DRV_ERROR_INVPARAM``
- not initialized / invalid lifecycle state: ``DRV_ERROR_INVSTATE``
- map/unmap failures: ``DRV_ERROR_MAPFAILED``
- temporary bus condition (busy, retry): ``DRV_ERROR_AGAIN``
- transfer timeout/NACK/error flags: ``DRV_ERROR_TIMEOUT``
- unsupported bus setup: ``DRV_ERROR_UNSUPPORTED_CFG``

Platform descriptor pattern
^^^^^^^^^^^^^^^^^^^^^^^^^^^

The driver still relies on ``struct platform_device_driver`` for registration,
mapping and IRQ dispatch:

.. code-block:: c

   static struct platform_device_driver my_i2c_driver = {
      .label      = 0U,
      .devh       = 0,
      .name       = "my i2c controller driver",
      .compatible = "vendor,soc-i2c",
      .devinfo    = NULL,
      .platform_fops = {
         .isr = stm32_i2c_isr,
      },
      .type = DEVICE_TYPE_I2C,
   };

Probe/init/release pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

``i2c_probe()`` only registers the instance using its DTS label:

.. code-block:: c

   drv_status_t i2c_probe(uint32_t label)
   {
      if (merlin_platform_driver_register(&my_i2c_driver, label) != STATUS_OK) {
         return DRV_ERROR_NOTREGISTERED;
      }
      return DRV_STATUS_OK;
   }

``i2c_init()`` maps MMIO, configures GPIO, then applies bus mode/speed:

.. code-block:: c

   drv_status_t i2c_init(uint32_t label,
                         enum i2c_speeds speed,
                         enum i2c_address_mode mode)
   {
      (void)label;

      if (merlin_platform_driver_map(&my_i2c_driver) != STATUS_OK) {
         return DRV_ERROR_MAPFAILED;
      }

      if (merlin_platform_driver_configure_gpio(&my_i2c_driver) != STATUS_OK) {
         return DRV_ERROR_CONFIGURATION;
      }

      if (stm32_i2c_bus_set_addressing_mode(mode) != 0) {
         return DRV_ERROR_UNSUPPORTED_CFG;
      }

      if (stm32_i2c_bus_apply_speed(speed) != 0) {
         return DRV_ERROR_UNSUPPORTED_CFG;
      }

      return DRV_STATUS_OK;
   }

``i2c_release()`` unmaps resources and returns a ``drv_status_t``:

.. code-block:: c

   drv_status_t i2c_release(uint32_t label)
   {
      (void)label;

      stm32_i2c_bus_disable();

      if (merlin_platform_driver_unmap(&my_i2c_driver) != STATUS_OK) {
         return DRV_ERROR_MAPFAILED;
      }

      return DRV_STATUS_OK;
   }

Read/write operations
^^^^^^^^^^^^^^^^^^^^^

The exported runtime calls ``i2c_write7/read7/write10/read10`` are expected to:

- validate the current addressing mode (7-bit vs 10-bit path);
- perform bounded polling (TXIS/RXNE/TC/STOPF);
- clear sticky error flags (NACK/BERR/ARLO/OVR);
- convert hardware outcomes into ``drv_status_t`` values.

Alias pattern (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

A driver implementation can keep vendor-prefixed internals and export the
unified API via aliases:

.. code-block:: c

   drv_status_t i2c_probe(uint32_t label)
      __attribute__((alias("stm32_i2c_driver_probe")));
   drv_status_t i2c_init(uint32_t label,
                         enum i2c_speeds speed,
                         enum i2c_address_mode mode)
      __attribute__((alias("stm32_i2c_driver_init")));
   drv_status_t i2c_write7(uint32_t label, uint8_t slave_addr, uint8_t reg_addr,
                           uint8_t *data, size_t length)
      __attribute__((alias("stm32_i2c_bus_write7")));
   drv_status_t i2c_read7(uint32_t label, uint8_t slave_addr, uint8_t reg_addr,
                          uint8_t *data, size_t length)
      __attribute__((alias("stm32_i2c_bus_read7")));
   drv_status_t i2c_write10(uint32_t label, uint16_t slave_addr, uint8_t reg_addr,
                            uint8_t *data, size_t length)
      __attribute__((alias("stm32_i2c_bus_write10")));
   drv_status_t i2c_read10(uint32_t label, uint16_t slave_addr, uint8_t reg_addr,
                           uint8_t *data, size_t length)
      __attribute__((alias("stm32_i2c_bus_read10")));
   drv_status_t i2c_release(uint32_t label)
      __attribute__((alias("stm32_i2c_driver_release")));
