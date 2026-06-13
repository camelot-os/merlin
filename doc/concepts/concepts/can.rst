CAN Driver Concepts
===================

.. index::
   single: CAN
   single: FDCAN
   single: Merlin; CAN API

This chapter summarizes the Merlin CAN model and the expectations for a
platform-backed CAN bus driver implementation.

Scope and goals
---------------

Merlin CAN support follows the same design principles used for other bus
families:

- keep platform discovery/mapping/GPIO setup in Merlin platform services;
- keep CAN controller register logic in the concrete driver;
- expose a stable family API to upper layers.

The public CAN contracts are split in two layers:

- data and configuration contracts in ``include/merlin/buses/can.h``;
- unified CAN API in ``include/merlin/platform/api/can.h``.

Unified CAN API
---------------

The user-facing API for CAN is:

- ``can_probe(label)``
- ``can_init(label, cfg)``
- ``can_send(label, frame)``
- ``can_recv(label, frame)``
- ``can_release(label)``

The runtime sequence is:

1. probe from DTS label,
2. init and hardware setup,
3. repeated send/recv operations,
4. release and resource unmap.

Calling runtime functions before successful probe/init should return
``DRV_ERROR_NOTREGISTERED`` or ``DRV_ERROR_INVSTATE``.

Data model
----------

``struct can_config`` defines controller configuration:

- operating mode (normal/listen/loopback variants),
- nominal bitrate,
- optional CAN-FD and bitrate-switching controls.

``struct can_frame`` defines transfer payload:

- identifier value and identifier type (standard/extended),
- frame format (classic/FD),
- RTR and BRS flags,
- DLC and payload bytes.

Even when a concrete example only enables a subset (for example 8-byte payload
polled flow), the API contract is stable and can be extended by future drivers.

Platform integration details
----------------------------

CAN drivers use the standard Merlin platform object:

.. code-block:: c

   static struct platform_device_driver my_can_driver = {
       .label = 0,
       .compatible = "vendor,can-controller",
       .type = DEVICE_TYPE_CAN,
       .platform_fops = {
           .isr = my_can_isr,
       },
   };

At probe time, Merlin resolves DTS metadata and fills ``devinfo``. Driver code
then derives register addresses from ``devinfo->baseaddr`` and uses
``merlin_ioread*``/``merlin_iowrite*`` for MMIO.

When bitrate programming depends on parent bus frequency, the driver should use
``merlin_platform_driver_get_bus_clock()``.

DeviceTree expectations
-----------------------

For CAN binding to succeed, DTS nodes should provide:

- a stable ``sentry,label`` used by ``can_probe``;
- a ``compatible`` value matching driver expectations;
- enabled/owned status compatible with task ownership checks.

CAN metadata generation also relies on the bus clock property used by the CAN
backend template (``can,peripheral-clock``), which is consumed by
``merlin_platform_driver_get_bus_clock()``.

Reference implementation
------------------------

The STM32U5 sample implementation is provided in:

- ``examples/can/stm32u5_fdcan_driver.c``

It illustrates:

- M_CAN/FDCAN register setup in init mode,
- polling-based TX/RX flow,
- error mapping to ``drv_status_t``,
- unified API symbol export through aliasing.
