Merlin principles
=================

Main goal
---------

Merlin is a user-space driver framework for Camelot-OS operating system.
It provides a unified and structured API for all user-space drivers
developers, to help with driver integration over the Camelot-OS API,
in order to accelerate device drivers development by reducing the required
implementation to the effective device-specific logic, leaving all the non-device
relative code to Merlin itself.

Using the same model as the Linux kernel platform driver framework, but with
a reduced complexity and footprint to comply with the constraints of microcontrollers,
Merlin ensure the following:

   * Device-tree relative metadata and accessors generation and abstraction
   * device memory mapping/unmapping and register access abstraction, allowing portable drivers over multiple SoC and memory maps
   * IRQ dispatching over one or more drivers, allowing distribution of events over
     multiples drivers in a single threaded, single blocking point execution loop
   * complete abstraction of all non-portable informations, such as device base address, IRQ numbers,
     GPIO pinmux configuration, etc.
   * delivering a unified API for each device family (I2C, SPI, USB, USART...) to ease
     device drivers development and reuse.


Such an architecture also allows to develop drivers with unified upper API so that a given
Camelot-OS application can be ported to various platform with various devices and devices drivers
without requiring any change in the application code.

Typical sequence and layer interactions
---------------------------------------

Merlin is made to support a layered API that allows applications to easily manipulate devices drivers,
and allows device drivers to interact between them if needed.

A typical complete driver hierarchy that include a peripheral bus driver and a bus controller driver
interacting over Merlin framework is shown in the following schema:

.. image:: ../ _static/figures/schema_block.png
   :align: center
   :width: 90%

Merlin delivers a usual device driver call sequence, that can be summarized as, for buses:

   * probing the bus controller
   * initializing the bus controller
   * probing the peripheral devices on the bus
   * initializing the peripheral devices
   * exchanging data with the peripheral devices
   * releasing the peripheral devices
   * releasing the bus controller

Memory management, device-tree related information are fully opaque to application and drivers
developers.

The following schema shows a usual sequence of calls and interactions between the bus controller driver, the peripheral device driver and the application:

.. image:: ../ _static/figures/schema_sequence.png
   :align: center
   :width: 90%

Note that this sequence is just an example of a possible interaction between the different layers, and that the actual sequence may vary depending on
the use case and the specific drivers involved.
The main point is that Merlin provides a unified API for all these interactions, allowing developers
to focus on the device-specific logic rather than the integration details.

Such a sequence is based on the following stepping stones:

   * platform driver registration and metadata resolution
   * platform driver device initialization and configuration
   * when using external devices through platform device (e.g. I2C, SPI...):
      * peripheral device probing and initialization
      * peripheral device data exchange
      * peripheral device release
   * when using autonomous platform device (e.g. CRYPTO, NPU...):
      * platform device data exchange
   * platform driver release

Merlin API layers
-----------------

Merlin exposes three complementary API layers:

- ``include/merlin/platform/driver.h``: platform abstraction for driver lifecycle
  (register, map, unmap, GPIO configuration, IRQ dispatch) over easy and abstracted API.
- ``include/merlin/buses/TYPE.h``: bus-specific operation contracts for various bus types
  (I2C read/write callbacks, SPI transfer callback, USB endpoint callbacks).
- ``include/merlin/io.h``: architecture-agnostic MMIO primitives
  (8/16/32-bit read/write and polling helpers). This primitives help with interacting with
  hardware registers without embedding architecture-specific assembly in the driver code.

This split keeps the hardware-dependent code focused on register semantics, while
device discovery, ownership checks, and runtime integration stay in Merlin.


Driver identity and metadata
----------------------------

Each platform driver is described using ``struct platform_device_driver``.
Key fields are:

- ``label``: device identifier matching ``sentry,label`` in the DTS.
- ``compatible``: hardware compatibility string used by the driver logic.
- ``type``: family selector (I2C, SPI, USB, ...), used by Merlin backend routing.
- ``devinfo``: pointer to DTS-derived metadata (base address, size, IRQs, GPIO pinmux).
- ``platform_fops.isr``: ISR callback invoked through Merlin IRQ dispatching.

The objective is to keep all dynamic platform context in one object that is owned
by the driver.

This fields are generic to all platform drivers.
For each device type, this structure is extended with type-specific fields, such as, for example,
the I2C read/write callbacks for an I2C bus driver, xfer for SPI, etc.
These API specifications are defined for each device type headers (e.g. ``include/merlin/buses/i2c.h``) and are
so that all drivers of a given family (e.g. all I2C bus drivers) share the same API contract, which
is then implemented in the bus-specific callbacks.

More concrete examples of this contract are defined for each bus types, with example drivers implementations
and examples driver usage from the application point of vue.

I/O abstraction principle
-------------------------

The MMIO accessors from ``include/merlin/io.h`` hide architecture-specific assembly
through ``include/merlin/arch/asm-generic/io.h``.

As a result, driver code can stay architecture-independent and use:

- ``merlin_ioread8/16/32()`` and ``merlin_iowrite8/16/32()``
- ``merlin_iopoll32_until_set()`` and ``merlin_iopoll32_until_clear()``

This interface is made in order to keep the device-specific code focused on register semantics,
while all the architecture-specific details stay in Merlin : memory barriers, ordering, and other
architecture-specific details are handled by the Merlin backend, allowing drivers to be portable
across different architectures without modification.

IRQ dispatch principle
----------------------

Most of the time, devices events are signaled through IRQs, and handling these events is a
critical part of the driver logic.

In Camelot-OS, IRQs are received by the kernel, and then dispatched to user-space. User-space applications
use an event loop over which events (including IRQs, but also for e.g. DMA events or IPCs) are received.
Because the application should not need to associated IRQs to device drivers it owns, Merlin
delivers a unified IRQ dispatching mechanism that allows to route IRQs to the right driver ISR callback,
using the device driver registered information and the DTS metadata resolved at registration time.

From the application point of vue, this is done using the ``merlin_platform_driver_irq_displatch()`` API.

This API takes the IRQ number received by the kernel as argument, and then performs the following steps:

   * resolve the target driver from DTS-backed metadata,
   * find the registered runtime driver context,
   * call the driver ISR callback through driver ISR exported symbol, that is responsible for:
      * read the device status and event registers
      * perform the device-related logic to handle the event
      * clear the device IRQ status to avoid re-triggering at device level
   * perform the central interrupt controller IRQ clear and re-enabling


A task handling multiple devices can therefore keep one event loop and let Merlin
perform IRQ-to-driver routing.

A typical basic application level for manipulating a device then looks like:


.. code-block:: c

   const struct usart_config uart3_cfg = {
      .baudrate     = 115200U,
      .mode         = USART_MODE_ASYNCHRONOUS,
      .parity       = USART_PARITY_NONE,
      .stop_bits    = USART_STOP_BITS_1,
      .word_length  = USART_WORD_LENGTH_8,
      .flow_control = USART_FLOW_CONTROL_NONE,
      .tx_enable    = true,
      .rx_enable    = true,
   };

   // registering driver against merlin
   if (usart_probe(USART3_LABEL) != 0) {
      goto err;
   }

   // initializing driver and device using merlin API
   if (usart_init(USART3_LABEL, &uart3_cfg) != 0) {
      goto err;
   }

   while (true) {
      // Wait for events (IRQs, DMA events, IPCs...)
      res = __sys_wait_for_event(EVENT_TYPE_IRQ, 20 /*timeout in ms*/);
      if (res != STATUS_OK) {
         // Handle wait error or timeout
         continue;
      }
      copy_from_kernel(event_buf, sizeof(event_buf));

      if (event.type == EVENT_TYPE_IRQ) {
         merlin_platform_driver_irq_dispatch(event.data[0] /*IRQ number*/);
      }

      // Handle received event data
      ...
   }
