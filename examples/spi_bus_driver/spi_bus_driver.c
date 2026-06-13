// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

/*
 * This file is a documentation-style template and is intentionally not part of
 * the build (see examples/meson.build).
 *
 * Keep the sketch in a disabled block so this file always remains valid C when
 * opened or accidentally compiled in isolation.
 */
#include <stddef.h>
#include <stdint.h>

#if 0

#include <merlin/buses/spi.h>

#define MY_SPI_BUS_LABEL CONFIG_SPI_ST32_LABEL

static int my_driver_xfer(struct platform_device_driver *self, uint8_t *rdbuf, uint8_t *wrbuf, size_t len)
{
	/* ... */
}

static int my_driver_probe(struct platform_device_driver *self, uint32_t label)
{
	/* ... */
}

static int my_driver_init(struct platform_device_driver *self)
{
	/* ... */
}

static int my_driver_release(struct platform_device_driver *self)
{
	/* ... */
}

static int my_driver_isr(struct platform_device_driver *self, uint32_t IRQn)
{
	/* ... */
}

static struct spi_bus_fops my_spi_specific_fops = {
	.xfer = my_driver_xfer,
};

static struct platform_device_driver my_spi_driver = {
	.label = MY_SPI_BUS_LABEL,
	.devh = 0,
	.name = "my SPI driver for XXX",
	.compatible = "st,stm32",
	.platform_fops = {
		.isr = my_driver_isr,
	},
};

/* natural automatic declaration at task startup, TBD (macro ?) */
merlin_platform_driver(&my_spi_driver);

/*
 * note: there is no need for declaring each IRQ as owned by the device as
 * the link between IRQn and devh_t is under the private control of Merlin,
 */

#endif
