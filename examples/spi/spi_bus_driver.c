// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

/*
 * A typical SPI bus driver needs only the following in its source code:
 */
#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>
#include <merlin/buses/spi.h>
#include <merlin/platform/api/spi.h>
#include <merlin/io.h>


/** @file Basic, single-controller driver */

#define MY_SPI_BUS_LABEL CONFIG_SPI_ST32_LABEL

/* STM32F4 SPI register map */
#define STM32_SPI_CR1_OFFSET     0x00UL
#define STM32_SPI_CR2_OFFSET     0x04UL
#define STM32_SPI_SR_OFFSET      0x08UL
#define STM32_SPI_DR_OFFSET      0x0CUL

/* SPI_CR1 bits */
#define STM32_SPI_CR1_CPHA       (1UL << 0)
#define STM32_SPI_CR1_CPOL       (1UL << 1)
#define STM32_SPI_CR1_MSTR       (1UL << 2)
#define STM32_SPI_CR1_BR_SHIFT   3U
#define STM32_SPI_CR1_BR_MASK    (7UL << STM32_SPI_CR1_BR_SHIFT)
#define STM32_SPI_CR1_SPE        (1UL << 6)
#define STM32_SPI_CR1_LSBFIRST   (1UL << 7)
#define STM32_SPI_CR1_SSI        (1UL << 8)
#define STM32_SPI_CR1_SSM        (1UL << 9)
#define STM32_SPI_CR1_RXONLY     (1UL << 10)
#define STM32_SPI_CR1_DFF        (1UL << 11)
#define STM32_SPI_CR1_BIDIOE     (1UL << 14)
#define STM32_SPI_CR1_BIDIMODE   (1UL << 15)

/* SPI_CR2 bits */
#define STM32_SPI_CR2_FRF        (1UL << 4)

/* SPI_SR bits */
#define STM32_SPI_SR_RXNE        (1UL << 0)
#define STM32_SPI_SR_TXE         (1UL << 1)
#define STM32_SPI_SR_BSY         (1UL << 7)

#define STM32_SPI_POLL_RETRIES   10000U

struct my_spi_private {
	struct spi_config cfg;
	size_t base;
	bool initialized;
	bool loopback;
};

static struct my_spi_private my_spi_ctx;

drv_status_t my_driver_set_cs(uint32_t label, uint8_t cs_index, bool active);

static inline size_t my_spi_reg(const struct my_spi_private *ctx, size_t offset)
{
	return ctx->base + offset;
}

static inline void my_spi_write32(const struct my_spi_private *ctx, size_t offset, uint32_t value)
{
	merlin_iowrite32(my_spi_reg(ctx, offset), value);
}

static drv_status_t my_spi_apply_sw_cs(const struct my_spi_private *ctx, bool active)
{
	bool nss_internal_high;
	uint32_t cr1;

	if (ctx == NULL) {
		return DRV_ERROR_INVSTATE;
	}

	/* In SSM mode, SSI drives the internal NSS level. */
	if (ctx->cfg.cs_polarity == SPI_CS_ACTIVE_LOW) {
		nss_internal_high = !active;
	} else {
		nss_internal_high = active;
	}

	cr1 = merlin_ioread32(my_spi_reg(ctx, STM32_SPI_CR1_OFFSET));
	cr1 |= STM32_SPI_CR1_SSM;
	if (nss_internal_high) {
		cr1 |= STM32_SPI_CR1_SSI;
	} else {
		cr1 &= ~STM32_SPI_CR1_SSI;
	}
	merlin_iowrite32(my_spi_reg(ctx, STM32_SPI_CR1_OFFSET), cr1);

	return DRV_STATUS_OK;
}

static uint32_t my_spi_compute_br_bits(uint32_t bus_hz, uint32_t speed_hz)
{
	static const uint16_t dividers[8] = {2U, 4U, 8U, 16U, 32U, 64U, 128U, 256U};
	uint32_t ratio;

	if (speed_hz == 0U) {
		return (7UL << STM32_SPI_CR1_BR_SHIFT);
	}

	ratio = (bus_hz + speed_hz - 1U) / speed_hz;
	if (ratio <= 2U) {
		return (0UL << STM32_SPI_CR1_BR_SHIFT);
	}

	for (uint32_t i = 0U; i < 8U; i++) {
		if (dividers[i] >= ratio) {
			return (i << STM32_SPI_CR1_BR_SHIFT);
		}
	}

	return (7UL << STM32_SPI_CR1_BR_SHIFT);
}

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
	 * allows to target directly the good controller instance and use associated
	 * private_data if needed
	 */
	(void)pdrv;
	(void)IRQn;
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
	drv_status_t res = DRV_STATUS_OK;
	uint32_t cr1 = 0UL;
	uint32_t cr2 = 0UL;
	uint32_t busfreq_mhz = 0UL;
	uint32_t bus_hz;
	bool loopback;

	if (config == NULL) {
		return DRV_ERROR_INVPARAM;
	}

	my_spi_ctx.initialized = false;
	my_spi_driver.private_data = NULL;

 	if (unlikely(merlin_platform_driver_map(&my_spi_driver) != STATUS_OK)) {
		return DRV_ERROR_MAPFAILED;
	}

	if ((my_spi_driver.devinfo == NULL) || (my_spi_driver.devinfo->baseaddr == 0UL)) {
		res = DRV_ERROR_CONFIGURATION;
		goto err_unmap;
	}
	my_spi_ctx.base = (size_t)my_spi_driver.devinfo->baseaddr;

	if (merlin_platform_driver_configure_gpio(&my_spi_driver) != STATUS_OK) {
		res = DRV_ERROR_CONFIGURATION;
		goto err_unmap;
	}

	if (merlin_platform_driver_get_bus_clock(&my_spi_driver, &busfreq_mhz) != STATUS_OK) {
		res = DRV_ERROR_CONFIGURATION;
		goto err_unmap;
	}
	bus_hz = busfreq_mhz * 1000000UL;

	/** Add driver local configuration infos to platform driver private data. */
	my_spi_ctx.cfg = *config;
	my_spi_ctx.loopback = false;

	if (config->word_size_bits == 16U) {
		cr1 |= STM32_SPI_CR1_DFF;
	} else if (config->word_size_bits != 8U) {
		res = DRV_ERROR_UNSUPPORTED_CFG;
		goto err_unmap;
	}

	loopback = (config->controller_mode == SPI_CONTROLLER_MODE_LOOPBACK);
	if ((config->controller_mode == SPI_CONTROLLER_MODE_MASTER) || loopback) {
		cr1 |= STM32_SPI_CR1_MSTR;
	} else if (config->controller_mode != SPI_CONTROLLER_MODE_SLAVE) {
		res = DRV_ERROR_UNSUPPORTED_CFG;
		goto err_unmap;
	}
	my_spi_ctx.loopback = loopback;

	if (config->cpol == SPI_CPOL_HIGH) {
		cr1 |= STM32_SPI_CR1_CPOL;
	}

	if (config->cpha == SPI_CPHA_CAPTURE_SECOND) {
		cr1 |= STM32_SPI_CR1_CPHA;
	}

	if (config->bit_order == SPI_BIT_ORDER_LSB_FIRST) {
		cr1 |= STM32_SPI_CR1_LSBFIRST;
	}

	if (config->frame_format == SPI_FRAME_FORMAT_TI) {
		cr2 |= STM32_SPI_CR2_FRF;
	}

	switch (config->duplex_mode) {
		case SPI_DUPLEX_FULL:
			break;
		case SPI_DUPLEX_HALF:
		case SPI_DUPLEX_SIMPLEX_TX:
			cr1 |= STM32_SPI_CR1_BIDIMODE | STM32_SPI_CR1_BIDIOE;
			break;
		case SPI_DUPLEX_SIMPLEX_RX:
			cr1 |= STM32_SPI_CR1_BIDIMODE;
			break;
		default:
			res = DRV_ERROR_UNSUPPORTED_CFG;
			goto err_unmap;
	}

	if (loopback) {
		/*
		 * Loopback tests require simultaneous TX and RX paths.
		 * Force the controller in 2-lines full-duplex mode.
		 */
		if (config->duplex_mode != SPI_DUPLEX_FULL) {
			res = DRV_ERROR_UNSUPPORTED_CFG;
			goto err_unmap;
		}
		cr1 &= ~(STM32_SPI_CR1_BIDIMODE | STM32_SPI_CR1_BIDIOE | STM32_SPI_CR1_RXONLY);
	}

	if (config->cs_management == SPI_CS_SOFTWARE) {
		cr1 |= STM32_SPI_CR1_SSM | STM32_SPI_CR1_SSI;
	}

	cr1 &= ~STM32_SPI_CR1_BR_MASK;
	cr1 |= my_spi_compute_br_bits(bus_hz, config->speed_hz);

	/* Program CR1/CR2 then enable SPI */
	my_spi_write32(&my_spi_ctx, STM32_SPI_CR1_OFFSET, cr1);
	my_spi_write32(&my_spi_ctx, STM32_SPI_CR2_OFFSET, cr2);
	my_spi_write32(&my_spi_ctx, STM32_SPI_CR1_OFFSET, (cr1 | STM32_SPI_CR1_SPE));

	if (config->cs_management == SPI_CS_SOFTWARE) {
		res = my_spi_apply_sw_cs(&my_spi_ctx, false);
		if (res != DRV_STATUS_OK) {
			goto err_unmap;
		}
	}

	my_spi_driver.private_data = &my_spi_ctx;
	my_spi_ctx.initialized = true;

	// configure SPI bus based on config parameters (speed, duplex, frame format, and so on)
 	// [...]

 	return DRV_STATUS_OK;

err_unmap:
	merlin_platform_driver_unmap(&my_spi_driver);
	my_spi_driver.private_data = NULL;
	my_spi_ctx.initialized = false;
	return res;
}

drv_status_t my_driver_set_cs(uint32_t label, uint8_t cs_index, bool active)
{
	struct my_spi_private *ctx = (struct my_spi_private *)my_spi_driver.private_data;

	(void)label;

	if (ctx == NULL || !ctx->initialized) {
		return DRV_ERROR_INVSTATE;
	}

	if (cs_index != ctx->cfg.cs_index) {
		return DRV_ERROR_INVPARAM;
	}

	if (ctx->cfg.cs_management != SPI_CS_SOFTWARE) {
		return DRV_ERROR_UNSUPPORTED;
	}

	return my_spi_apply_sw_cs(ctx, active);
}

static drv_status_t my_driver_xfer(uint32_t label, uint8_t *rdbuf, const uint8_t *wrbuf, size_t len)
{
	struct my_spi_private *ctx = (struct my_spi_private *)my_spi_driver.private_data;
	bool do_read;
	bool do_write;

	(void)label;

	if (ctx == NULL || !ctx->initialized) {
		return DRV_ERROR_INVSTATE;
	}

	if (len == 0U) {
		return DRV_STATUS_OK;
	}

	if (rdbuf == NULL && wrbuf == NULL) {
		return DRV_ERROR_INVPARAM;
	}

	do_write = (wrbuf != NULL) ||
		(ctx->cfg.duplex_mode == SPI_DUPLEX_FULL) ||
		(ctx->cfg.duplex_mode == SPI_DUPLEX_SIMPLEX_TX) ||
		(ctx->cfg.duplex_mode == SPI_DUPLEX_HALF);

	do_read = (rdbuf != NULL) ||
		(ctx->cfg.duplex_mode == SPI_DUPLEX_FULL) ||
		(ctx->cfg.duplex_mode == SPI_DUPLEX_SIMPLEX_RX);

	/* In master mode, RX-only transfers still require dummy writes to generate clock. */
	if (!do_write && do_read && (ctx->cfg.controller_mode != SPI_CONTROLLER_MODE_SLAVE)) {
		do_write = true;
	}

	for (size_t i = 0U; i < len; i++) {
		if (do_write) {
			uint8_t tx = (wrbuf != NULL) ? wrbuf[i] : 0xFFU;
			if (merlin_iopoll32_until_set(my_spi_reg(ctx, STM32_SPI_SR_OFFSET),
			                             STM32_SPI_SR_TXE,
			                             STM32_SPI_POLL_RETRIES) != STATUS_OK) {
				return DRV_ERROR_TIMEOUT;
			}
			merlin_iowrite8(my_spi_reg(ctx, STM32_SPI_DR_OFFSET), tx);
		}

		if (do_read) {
			uint8_t rx;
			if (merlin_iopoll32_until_set(my_spi_reg(ctx, STM32_SPI_SR_OFFSET),
			                             STM32_SPI_SR_RXNE,
			                             STM32_SPI_POLL_RETRIES) != STATUS_OK) {
				return DRV_ERROR_TIMEOUT;
			}
			rx = merlin_ioread8(my_spi_reg(ctx, STM32_SPI_DR_OFFSET));
			if (rdbuf != NULL) {
				rdbuf[i] = rx;
			}
		}
	}

	if (merlin_iopoll32_until_clear(my_spi_reg(ctx, STM32_SPI_SR_OFFSET),
	                               STM32_SPI_SR_BSY,
	                               STM32_SPI_POLL_RETRIES) != STATUS_OK) {
		return DRV_ERROR_TIMEOUT;
	}

	return DRV_STATUS_OK;
}

static drv_status_t my_driver_release(uint32_t label)
{
	(void)label;

	/* deactivate SPI bus controller */
	merlin_platform_driver_unmap(&my_spi_driver);
 	return DRV_STATUS_OK;
}

drv_status_t spi_probe(uint32_t label) __attribute__((alias("my_driver_probe")));
drv_status_t spi_init(uint32_t label, struct spi_config *config) __attribute__((alias("my_driver_init")));
drv_status_t spi_xfer(uint32_t label, uint8_t *rdbuf, const uint8_t *wrbuf, size_t len) __attribute__((alias("my_driver_xfer")));
drv_status_t spi_release(uint32_t label) __attribute__((alias("my_driver_release")));
drv_status_t spi_set_cs(uint32_t label, uint8_t cs_index, bool active) __attribute__((alias("my_driver_set_cs")));
