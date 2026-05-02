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
