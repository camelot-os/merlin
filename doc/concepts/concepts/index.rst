Merlin Concepts
===============

Merlin provides a layered userspace driver model for Camelot-OS.
The framework centralizes platform services (device discovery, MMIO mapping,
GPIO pin configuration, and IRQ routing) so driver code can focus on hardware
logic and protocol behavior.

This section documents the architectural contracts that are shared by all
Merlin-based drivers:

- the public C interfaces exported from ``include/merlin``;
- the expected probe/init/runtime/release lifecycle;
- the DeviceTree properties required to bind runtime code to generated metadata.

.. toctree::
   :maxdepth: 2
   :caption: Contents:

   principles.rst
   usage.rst
   dts.rst
