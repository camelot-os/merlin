<!-- SPDX-License-Identifier: Apache-2.0 -->
<!-- Copyright (c) 2026 H2Lab Development Team -->

# Merlin driver framework for Camelot user drivers

<img src="doc/concepts/_static/figures/merlin.png" width="200">

## Basics

Merlin is a driver framework to unify as much as possible the common actions and tooling requires for all drivers.

Merlin is responsible for typical actions:

- forge the task declared devices list based on the input device tree
- deliver various helpful getters on various DTS or SVD related fields, in a generic way for any SoC/Arch
- allow easy driver registering and usage, making the driver developer to implement only the HW IP specific code

## Merlin architectural model

All in-SoC devices are regrouped using the plaform interface of Merlin, declared in the `<merlin/platform/driver.h>`
header. Various in-S devices such as buses are based on this interface for their declaration

## Examples

A sample SPI driver is defined in the `examples` directory to show typical way to use Merlin.
