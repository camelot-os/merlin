DTS and dt-bindings
===================

.. index::
   single: DTS
   single: dt-bindings
   single: dts-include-dirs

DeviceTree files are preprocessed before metadata generation. Any
``#include <...>`` statement in a DTS file is resolved from the include search
path given to the preprocessor.

In Merlin, this path is extended with ``-Ddts-include-dirs=...``.
This option is an array of directories appended as ``-I`` flags during DTS
preprocessing, so headers and ``.dtsi`` fragments can be found.

Typical usage:

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
----------------------------------------

.. index::
   single: DTS; I2C controller node
   single: sentry,label
   single: sentry,owner
   single: i2c,hsifreq
   single: i2c,peripheral-clock

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
-------------------------------------------------

.. index::
   single: DTS; I2C peripheral node
   single: devinfo_t

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
-------------------------------

.. index::
   single: DTS; USB node
   single: reg
   single: interrupts
   single: maximum-speed

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
----------------------------------

.. index::
   single: pinctrl-0
   single: pinmux
   single: pincfg
   single: STM32_DT_PIN_MODE_ALTFUNC
   single: STM32_OTYPER_OPEN_DRAIN
   single: STM32_PUPDR_PULL_UP

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
^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: merlin_platform_driver_register()
   single: merlin_platform_driver_map()
   single: merlin_platform_driver_unmap()
   single: merlin_platform_driver_configure_gpio()
   single: merlin_platform_driver_irq_displatch()
   single: platform_device_driver
   single: devinfo_t
   single: devh_t

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
^^^^^^^^^^^^^^^^^^^^^^^^^^

.. index::
   single: CONFIG_WITH_MERLIN
   single: CONFIG_MAX_REGISTERED_DRIVERS
   single: CONFIG_CAP_DEV_BUSES
   single: CONFIG_CAP_DEV_IO

The sample configuration enables the Merlin runtime prerequisites:

- ``CONFIG_WITH_MERLIN=y``
- ``CONFIG_MAX_REGISTERED_DRIVERS=4``
- ``CONFIG_CAP_DEV_BUSES=y``
- ``CONFIG_CAP_DEV_IO=y``

These settings allow registration, MMIO mapping, and GPIO configuration through
Merlin APIs.
