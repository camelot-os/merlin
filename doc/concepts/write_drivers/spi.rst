Merlin-based SPI driver
-----------------------

.. index::
   single: SPI; driver
   single: include/merlin/platform/api/spi.h
   single: drv_status_t

This section describes the expected contract for a Merlin SPI bus controller
driver.

Public API contract
^^^^^^^^^^^^^^^^^^^

The exported API is defined in ``include/merlin/platform/api/spi.h``:

- ``spi_probe(uint32_t label)``
- ``spi_init(uint32_t label, struct spi_config *config)``
- ``spi_xfer(uint32_t label, uint8_t *rdbuf, const uint8_t *wrbuf, size_t len)``
- ``spi_set_cs(uint32_t label, uint8_t cs_index, bool active)``
- ``spi_release(uint32_t label)``

SPI configuration types (mode, baudrate, frame format, bit order and CS
policy) are provided by ``include/merlin/buses/spi.h`` through
``struct spi_config``.

Legacy callback-table interfaces are no longer the public interface.

Status model
^^^^^^^^^^^^

All exported SPI symbols return ``drv_status_t``.

Recommended conversion from low-level outcomes:

- invalid ``config`` or inconsistent parameters: ``DRV_ERROR_INVPARAM``
- unsupported frame format/mode/divider: ``DRV_ERROR_UNSUPPORTED_CFG``
- invalid driver lifecycle state: ``DRV_ERROR_INVSTATE``
- map/unmap/register failures: ``DRV_ERROR_MAPFAILED`` or
  ``DRV_ERROR_NOTREGISTERED``
- transfer timeout/FIFO stall/bus contention: ``DRV_ERROR_TIMEOUT`` or
  ``DRV_ERROR_AGAIN``

Platform descriptor pattern
^^^^^^^^^^^^^^^^^^^^^^^^^^^

SPI drivers are implemented as platform drivers and expose ISR hooks if needed:

.. code-block:: c

   static struct platform_device_driver my_spi_driver = {
      .label      = 0U,
      .devh       = 0,
      .name       = "my spi controller driver",
      .compatible = "vendor,soc-spi",
      .devinfo    = NULL,
      .platform_fops = {
         .isr = my_spi_isr,
      },
      .type = DEVICE_TYPE_SPI,
   };

Probe/init/xfer/release pseudo-code
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   drv_status_t spi_probe(uint32_t label)
   {
      if (merlin_platform_driver_register(&my_spi_driver, label) != STATUS_OK) {
         return DRV_ERROR_NOTREGISTERED;
      }
      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t spi_init(uint32_t label, struct spi_config *config)
   {
      (void)label;

      if (config == NULL) {
         return DRV_ERROR_INVPARAM;
      }

      if (merlin_platform_driver_map(&my_spi_driver) != STATUS_OK) {
         return DRV_ERROR_MAPFAILED;
      }

      if (my_spi_apply_config(config) != 0) {
         return DRV_ERROR_UNSUPPORTED_CFG;
      }

      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t spi_xfer(uint32_t label,
                         uint8_t *rdbuf,
                         const uint8_t *wrbuf,
                         size_t len)
   {
      (void)label;

      if (len == 0U || (rdbuf == NULL && wrbuf == NULL)) {
         return DRV_ERROR_INVPARAM;
      }

      if (my_spi_wait_ready() != 0) {
         return DRV_ERROR_TIMEOUT;
      }

      return DRV_STATUS_OK;
   }

.. code-block:: c

   drv_status_t spi_release(uint32_t label)
   {
      (void)label;

      if (merlin_platform_driver_unmap(&my_spi_driver) != STATUS_OK) {
         return DRV_ERROR_MAPFAILED;
      }

      return DRV_STATUS_OK;
   }

Chip-select handling
^^^^^^^^^^^^^^^^^^^^

``spi_set_cs()`` can be implemented either via hardware CS support in the SPI
controller or via GPIO management performed by the platform layer.

When unsupported on a given target, return ``DRV_ERROR_UNSUPPORTED_CFG``.

Alias pattern (recommended)
^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: c

   drv_status_t spi_probe(uint32_t label)
      __attribute__((alias("my_spi_driver_probe")));
   drv_status_t spi_init(uint32_t label, struct spi_config *config)
      __attribute__((alias("my_spi_driver_init")));
   drv_status_t spi_xfer(uint32_t label, uint8_t *rdbuf,
                         const uint8_t *wrbuf, size_t len)
      __attribute__((alias("my_spi_driver_xfer")));
   drv_status_t spi_set_cs(uint32_t label, uint8_t cs_index, bool active)
      __attribute__((alias("my_spi_driver_set_cs")));
   drv_status_t spi_release(uint32_t label)
      __attribute__((alias("my_spi_driver_release")));
