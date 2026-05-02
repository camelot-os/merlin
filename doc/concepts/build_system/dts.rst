Device tree usage
-----------------

.. index::
  single: dts

Overview
""""""""

Merlin consumes DTS data at build time and exposes it to drivers through
``struct platform_device_driver`` at runtime.

The objective is to keep board-specific values (addresses, interrupts, pinmux,
ownership) in DTS and avoid hard-coding these values in driver C code.

When building Merlin in a project, the user provides a DTS file describing the
hardware layout and a Kconfig configuration describing the task's ownership of devices.
The Merlin build system then needs the following arguments:

  * `-Ddts=path/to/project_dts.dts`
  * `-Ddts-include-dirs=path/to/dts_includes/`
  * `-Dconfig=path/to/application_config.config`

Merlin automatically filter the device tree file to genrate C code for the task owned devices
only. This allows the same DTS file to be used across multiple tasks with different ownership,
without any manual edits to the DTS or C code.
This behavior is standard in the Camelot-OS operating system, as the device tree file is a part
of the project's common configuration, shared across all tasks.
The task only requires to know the labels of the devices it owns, which are defined in the DTS
file and used as identifiers in the application code that manipulate userspace drivers.

The application developer is free to decide the way devices labels are shared with the DTS file
(hardcoded in the application code, using the application Kconfig or any other way).

When using Camelot-OS project with a Camelot SDK, the include-dir is automatically resolved
from the SDK path, and do not require any user input.

Sample Input artifacts
""""""""""""""""""""""

The are some reference inputs in this repository to help understand the expected format of the DTS
and Kconfig files:

- ``dts/sample.dts``
- ``configs/sample.config``

The key linkage between the two files is:

- ``CONFIG_TASK_LABEL=0xC001F002`` in ``sample.config``
- ``sentry,owner = <0xC001F002>`` in ``sample.dts``

Only devices owned by the task are selected in generated Merlin metadata.


From DTS to generated C metadata

DTS and dt-bindings
"""""""""""""""""""

DeviceTree files are preprocessed before metadata generation. Any
``#include <...>`` statement in a DTS file is resolved from the include search
path given to the preprocessor.

In Merlin, this path is extended with ``-Ddts-include-dirs=...``.
This option is an array of directories appended as ``-I`` flags during DTS
preprocessing, so headers and ``.dtsi`` fragments can be found.

Typical usage in ``sample.dts``:

.. code-block:: dts

   #include <dt-bindings/pinctrl/stm32-pinctrl.h>

Why this is needed:

- the macros used in ``pinmux`` and ``pincfg`` (for example
  ``STM32_DT_PIN_MODE_ALTFUNC(4)``, ``STM32_OTYPER_OPEN_DRAIN``,
  ``STM32_PUPDR_PULL_UP``) are defined in dt-binding headers,
- without a directory in the include path exposing ``dt-bindings/...``,
  preprocessing fails before Merlin can generate C metadata.

In Camelot SDK integration, bindings are hosted under
``SDK_ROOT/include/dt-bindings`` for the selected target platform.
This layout typically includes, for example when targeting an STM32 SoC:

.. code-block:: none

   include/
   ├── dt-bindings/
   │   ├── pinctrl/
   │   │   ├── stm32-pinctrl.h
   │   │   ├── stm32-pinctrl-common.h
   │   │   └── ...
   │   ├── clock/
   │   ├── reset/
   │   ├── gpio/
   │   ├── spi/
   │   └── ...
   ├── device.h
   ├── platform.h
   └── ...

For ``#include <dt-bindings/pinctrl/stm32-pinctrl.h>``, the include root must
be a directory containing ``dt-bindings`` as a child. In practice, this is
typically ``SDK_ROOT/include`` (resolved automatically when using Camelot SDK
project integration).

The same mechanism also applies to ``.dtsi`` includes (architecture,
manufacturer, board fragments): ``-Ddts-include-dirs`` defines where the
preprocessor searches for these files.

For I2C devices, Merlin uses the template ``src/buses/i2c/i2c_dt.h.in``.
For USB devices, Merlin uses the template ``src/buses/usb/usb_dt.h.in``.

During build, each template is rendered into a generated header
(``builddir/src/buses/i2c/i2c_dt.h`` for I2C,
``builddir/src/buses/usb/usb_dt.h`` for USB).

The following real snippets from ``dts/sample.dts`` show the expected
DeviceTree syntax used by Merlin's generators.



I2C controller node (bus-level metadata)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: dts

   &i2c1 {
     status = "okay";
     compatible = "st,stm32u5-i2c";
     sentry,owner = <0xC001F002>;
     sentry,label = <0x100>;
     i2c,hsifreq = <40>;
     i2c,peripheral-clock = <40>; /* 40MHz APB1 clock */
     pinctrl-0 = <&i2c1_scl_pb8>, <&i2c1_sda_pb9>;
   };

From this node, Merlin extracts:

- ``id`` from ``sentry,label`` (``0x100``),
- ownership filter from ``sentry,owner`` (must match ``CONFIG_TASK_LABEL``),
- GPIO/pinmux descriptors from ``pinctrl-0``.

I2C child peripheral node (device-level metadata)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: dts

   &i2c1 {
     touch {
       status = "okay";
       compatible = "ilitech,ili2130";
       sentry,owner = <0xC001F002>;
       sentry,label = <0x101>;
       pinctrl-0 = <&touch_rstn_pa0>, <&touch_intn_in>, <&touch_intn_out>;
     };
   };

This produces one generated ``devinfo_t`` entry for label ``0x101`` when the
owner matches the task label.

USB node (MMIO + IRQ + pinctrl)
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

.. code-block:: dts

   /{
     soc {
       usbotg_fs: otgfs@42040000 {
         compatible = "st,stm32-otgfs";
         reg = <0x42040000 0x80000>;
         interrupts = <73 0>;
         interrupt-names = "otgfs";
         num-bidir-endpoints = <6>;
         ram-size = <1280>;
         maximum-speed = "full-speed";
         pinctrl-names = "default";
         pinctrl-0 = <&usb_otg_fs_dm_pa11>, <&usb_otg_fs_dp_pa12>, <&usb_otg_fs_vbus_pa9>;
         sentry,owner = <0xC001F002>;
         sentry,label = <0x102>;
         status = "okay";
       };
     };
   };

From this node, Merlin extracts:

- ``baseaddr`` and ``size`` from ``reg``,
- interrupt list from ``interrupts``,
- ``id`` from ``sentry,label`` (``0x102``),
- pin configuration references from ``pinctrl-0``.

How ``pinctrl-0`` configures GPIOs
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

In Merlin DTS inputs, ``pinctrl-0`` is a list of phandle references to pin
configuration nodes declared under ``&pinctrl``.

For example, the I2C bus node references two pin states:

.. code-block:: dts

   &i2c1 {
     pinctrl-0 = <&i2c1_scl_pb8>, <&i2c1_sda_pb9>;
   };

These references are resolved in ``&pinctrl``:

.. code-block:: dts

   &pinctrl {
     i2c1_scl_pb8: i2c_scl_pb8 {
       id = "i2c1_scl";
       pinmux = <&gpiob 8 STM32_DT_PIN_MODE_ALTFUNC(4)>;
       pincfg = <STM32_OTYPER_OPEN_DRAIN \
                 STM32_OSPEEDR_VERY_HIGH_SPEED \
                 STM32_PUPDR_PULL_UP>;
     };

     i2c1_sda_pb9: i2c_sda_pb9 {
       id = "i2c1_sda";
       pinmux = <&gpiob 9 STM32_DT_PIN_MODE_ALTFUNC(4)>;
       pincfg = <STM32_OTYPER_OPEN_DRAIN \
                 STM32_OSPEEDR_VERY_HIGH_SPEED \
                 STM32_PUPDR_PULL_UP>;
     };
   };

The same pattern is used for the touch controller GPIO lines:

.. code-block:: dts

   &i2c1 {
     touch {
       pinctrl-0 = <&touch_rstn_pa0>, <&touch_intn_in>, <&touch_intn_out>;
     };
   };

.. code-block:: dts

   &pinctrl {
     touch_rstn_pa0: touch_rstn_pa0 {
       id = "touch_rstn";
       pinmux = <&gpioa 0 STM32_DT_PIN_MODE_OUTPUT>;
       pincfg = <STM32_OTYPER_PUSH_PULL \
                 STM32_OSPEEDR_VERY_HIGH_SPEED \
                 STM32_PUPDR_PULL_UP>;
     };

     touch_intn_in: touch_intn_pa1 {
       id = "touch_intn";
       pinmux = <&gpioa 1 STM32_DT_PIN_MODE_INPUT>;
       pincfg = <STM32_PUPDR_PULL_UP>;
     };

     touch_intn_out: touch_intn_pb0 {
       id = "touch_intn";
       pinmux = <&gpiob 0 STM32_DT_PIN_MODE_OUTPUT>;
       pincfg = <STM32_OTYPER_PUSH_PULL \
                 STM32_OSPEEDR_VERY_HIGH_SPEED \
                 STM32_PUPDR_PULL_UP>;
     };
   };

Syntax summary:

- ``pinctrl-0 = <&node_a>, <&node_b>;`` selects one or more pin definitions,
- ``label: name { ... };`` declares a pinctrl node and exposes ``label`` for
  phandle references,
- ``pinmux = <&gpioX pin MODE>;`` binds port, pin number, and function,
- ``pincfg = <...>;`` sets electrical properties (pull-up/down, speed,
  output type).

In summary, generated metadata tables contain one entry per active and owned
node, and each entry carries the information needed at runtime to register,
map, and configure the corresponding device through Merlin APIs.


Runtime binding in Merlin
"""""""""""""""""""""""""

At runtime, ``merlin_platform_driver_register()`` links driver code to DTS data:

1. select backend from ``driver->type`` (I2C and USB paths currently implemented),
2. resolve ``devinfo`` from the DTS-generated tables using the provided label,
3. request and store the kernel device handle,
4. persist the registered driver context for later IRQ dispatch.

After registration, the driver can call:

- ``merlin_platform_driver_map()`` / ``merlin_platform_driver_unmap()``
- ``merlin_platform_driver_configure_gpio()``
- ``merlin_platform_driver_irq_displatch()``

without manually re-parsing DTS data.


Configuration requirements
""""""""""""""""""""""""""

The sample configuration enables the Merlin runtime prerequisites:

- ``CONFIG_WITH_MERLIN=y``
- ``CONFIG_MAX_REGISTERED_DRIVERS=4``
- ``CONFIG_CAP_DEV_BUSES=y``
- ``CONFIG_CAP_DEV_IO=y``

These settings allow registration, MMIO mapping, and GPIO configuration through
Merlin APIs.
