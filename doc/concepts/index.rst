.. _merlin:

.. highlight:: c

Welcome to Merlin documentation!
********************************

.. toctree::
   :maxdepth: 3
   :caption: Contents:

   concepts/index.rst
   build_system/index.rst
   tests/index.rst

.. toctree::
   :hidden:

   genindex

.. image:: _static/figures/merlin.png
   :align: center
   :width: 30%


This is the Merlin driver framework architecture and API documentation.
The Merlin framework is an efficient userspace driver helper library made
in order to help with Camelot-OS userspace driver implementation.

Its goal is to simplify the way drivers developper interact with the Operating System,
so that only the device-related implementation is required when writing a device driver.

In order to achieve that, all the platform generic function are hold by Merlin itself.
This, for example, include the probing system, the device-tree related informations manipulation,
or the way IRQ events are dispatch toward the (potentially various) userspace ISRs implementations.

The Merlin driver framework is a part of the Camelot Operating System, built
to deploy a high level security upto the micro-controller, ensuring
secure manipulation of complex or performance critical I/O the SE is
not able to handle.

.. image:: _static/figures/camelot.png
   :align: right
   :width: 5%
