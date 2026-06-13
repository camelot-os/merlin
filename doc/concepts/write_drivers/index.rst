Writing Merlin Drivers
======================

This chapter describes the expected source layout and software contract for
Merlin drivers.

For all bus families, the recommended model is:

- declare public, application-facing prototypes in
   ``include/merlin/platform/api/*.h``;
- keep bus-specific enums and configuration structures in
   ``include/merlin/buses/*.h``;
- use ``drv_status_t`` (from ``include/merlin/platform/api/generic.h``) as
   the return type of exported driver APIs.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   usart.rst
   spi.rst
   i2c.rst
   usb.rst
