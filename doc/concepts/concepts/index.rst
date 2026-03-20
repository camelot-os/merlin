Merlin Concepts
===============

Merlin is a userspace driver framework that isolates the platform boilerplate from
the hardware IP implementation.

In practice, a driver is expected to implement only the device-specific logic
(register programming, transfer sequences, protocol constraints), while Merlin
provides common primitives for registration, mapping, GPIO setup, and IRQ dispatch.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   principles.rst
   usage.rst
