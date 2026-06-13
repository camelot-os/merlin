DTS and dt-bindings
===================

.. index::
   single: DTS
   single: dt-bindings
   single: sentry,label
   single: sentry,owner

Merlin runtime behavior is driven by metadata generated from DeviceTree.
The registration API does not accept base addresses or IRQ tables directly;
it accepts only a label and resolves everything else from generated DTS-backed
data.

Why DTS is a hard contract
--------------------------

``merlin_platform_driver_register()`` binds driver code to a DTS node through
``sentry,label`` and ownership constraints.

If DTS data is incomplete or inconsistent, probe or init fails even when the
driver code itself is correct.

At minimum, each active node used by Merlin must provide:

- ``status = "okay"``;
- ``compatible`` matching the intended driver family;
- ``sentry,label`` as unique runtime identifier;
- ``sentry,owner`` matching task ownership policy;
- required bus properties (for example ``reg``/``interrupts`` for MMIO
  controllers).

DTS preprocessing and include paths
-----------------------------------

Merlin DTS processing supports ``#include <...>`` directives for dt-bindings
and DTS fragments.

Use ``-Ddts-include-dirs=...`` to extend preprocessor include roots. This is
required when pinmux/pinctrl macros come from platform binding headers such as:

.. code-block:: dts

   #include <dt-bindings/pinctrl/stm32-pinctrl.h>

If these include roots are missing, metadata generation fails before C code is
compiled.

Controller node example (I2C)
-----------------------------

.. index::
   single: I2C DTS node
   single: pinctrl-0

.. code-block:: dts

   &i2c1 {
     status = "okay";
     compatible = "st,stm32u5-i2c";
     sentry,owner = <0xC001F002>;
     sentry,label = <0x100>;
     i2c,hsifreq = <40>;
     i2c,peripheral-clock = <40>;
     pinctrl-0 = <&i2c1_scl_pb8>, <&i2c1_sda_pb9>;
   };

This node provides enough data for the I2C bus driver sample to:

- probe by label,
- map controller registers,
- configure GPIO pinmux,
- derive timing inputs.

External child node example (I2C peripheral)
--------------------------------------------

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

The child node allows external-device logic to stay separate from controller
logic while still using unified Merlin registration and pin configuration APIs.

USB controller node example
---------------------------

.. code-block:: dts

   /{
     soc {
       usbotg_fs: otgfs@42040000 {
         compatible = "st,stm32-otgfs";
         reg = <0x42040000 0x80000>;
         interrupts = <73 0>;
         num-bidir-endpoints = <6>;
         ram-size = <1280>;
         maximum-speed = "full-speed";
         pinctrl-0 = <&usb_otg_fs_dm_pa11>, <&usb_otg_fs_dp_pa12>, <&usb_otg_fs_vbus_pa9>;
         sentry,owner = <0xC001F002>;
         sentry,label = <0x102>;
         status = "okay";
       };
     };
   };

The USB sample driver uses these fields to initialize core registers, FIFO
layout, endpoint capabilities, and IRQ handling.

Pinctrl model used by Merlin
----------------------------

``pinctrl-0`` is interpreted as a list of phandles to pin state nodes.
Each referenced node contributes pinmux and electrical configuration data used
during ``merlin_platform_driver_configure_gpio()``.

Typical syntax:

- ``pinctrl-0 = <&state_a>, <&state_b>;``
- ``state_label: state_name { ... };``
- ``pinmux = <&gpioX pin MODE>;``
- ``pincfg = <...>;``

Runtime binding sequence
------------------------

.. index::
   single: merlin_platform_driver_register()
   single: merlin_platform_driver_map()
   single: merlin_platform_driver_configure_gpio()

After successful ``merlin_platform_driver_register(driver, label)``:

1. ``driver->devinfo`` references generated metadata for that label;
2. ``driver->devh`` is associated with kernel-side device identity;
3. subsequent calls can map memory and configure GPIO without additional DTS
   parsing in driver code.

This is why Merlin driver APIs intentionally take labels and typed configuration
structures, not raw platform addresses.

Configuration prerequisites
---------------------------

The sample setup enables core Merlin capabilities with:

- ``CONFIG_WITH_MERLIN=y``
- ``CONFIG_MAX_REGISTERED_DRIVERS`` sized for expected concurrency
- ``CONFIG_CAP_DEV_BUSES=y``
- ``CONFIG_CAP_DEV_IO=y``

Without these options, registration, mapping, or GPIO configuration may not be
available at runtime.
