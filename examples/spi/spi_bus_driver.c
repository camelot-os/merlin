// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

/*
 * a typical SPI bus driver then needs only the following in its source code:
 */
#include <inttypes.h>
#include <stddef.h>
#include <merlin/buses/spi.h>
#include <merlin/platform/api/spi.h>
#include <merlin/io.h>


/** @file basic, single controller driver */

#define MY_SPI_BUS_LABEL CONFIG_SPI_ST32_LABEL

static struct platform_device_driver my_spi_driver = {
 	.label = 0,
	.devh = 0, // to be updated by driver probe
 	.name = "my SPI driver for XXX",
 	.compatible = "st,stm32", /*< maybe a table instead, like Linux */
	.platform_fops = {
		.isr = NULL,
	},
	.type = DEVICE_TYPE_SPI,
	.private_data = NULL, /*< to be used by the driver implementation if needed */
};

static int my_driver_isr(void *self, uint32_t IRQn)
{
	struct platform_device_driver *pdrv = (struct platform_device_driver *)self;
	/*
	 * no usage by now here, when supporting multiple controller in the same driver,
	 * allows to target directly the good controler instance and use associated
	 * private_data if needed
	 */
	return DRV_STATUS_OK;
}

static drv_status_t my_driver_probe(uint32_t label)
{
	drv_status_t res;

	my_spi_driver.platform_fops.isr = my_driver_isr;
	my_spi_driver.label = label;

	if (unlikely(merlin_platform_driver_register(&my_spi_driver, label) != STATUS_OK)) {
		res = DRV_ERROR_NOTREGISTERED;
		goto end;
	}
	res = DRV_STATUS_OK;
end:
	return res;
}

static drv_status_t my_driver_init(uint32_t label, struct spi_config *config)
{
 	if (unlikely(merlin_platform_driver_map(&my_spi_driver) != STATUS_OK)) {
		return DRV_ERROR_MAPFAILED;
	}
	/** adding driver local configuration infos to platform driver private data */
	my_spi_driver.private_data = config;

	// configure SPI bus based on config parameters (speed, duplex, frame format, and so on)
 	// [...]

 	return DRV_STATUS_OK;
}



static drv_status_t my_driver_xfer(uint32_t label, uint8_t *rdbuf, uint8_t *wrbuf, size_t len) {
 	// [...]
	return DRV_STATUS_OK;
}

static drv_status_t my_driver_release(uint32_t label)
{
	/* deactivate SPI bus controller */
	merlin_platform_driver_unmap(&my_spi_driver);
 	return DRV_STATUS_OK;
}
