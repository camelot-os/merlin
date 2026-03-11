// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

/*
 * This file is an example of a I2C bus driver implementation based on the merlin platform.
 * This driver is based on the STM32 I2C bus controller, but the same principles can be applied
 * to any other I2C bus controller.
 * The driver is implemented as a platform driver, which means that it is registered in the platform
 * and can be probed and initialized by the hosting application.
 * The driver license is BSD-3-Clause, which means that it can be used and modified freely, as
 * long as the original copyright notice and disclaimer are retained.
 */

#include <inttypes.h>
#include <stddef.h>
#include <merlin/buses/i2c.h>
#include <merlin/io.h>
#include "i2c_bus_driver.h"

/*
 * STM32 I2C bus controller relative registers address and fields definition
 */
#define I2C_CR1_OFFSET 0x00
#define I2C_CR2_OFFSET 0x04
#define I2C_OAR1_OFFSET 0x08
#define I2C_OAR2_OFFSET 0x0C
#define I2C_TIMINGR_OFFSET 0x10
#define I2C_TIMEOUTR_OFFSET 0x14
#define I2C_ISR_OFFSET 0x18
#define I2C_ICR_OFFSET 0x1C
#define I2C_PECR_OFFSET 0x20
#define I2C_RXDR_OFFSET 0x24
#define I2C_TXDR_OFFSET 0x28

#define I2C_CR1_PE (1UL << 0)
#define I2C_CR1_TXIE (1UL << 1)
#define I2C_CR1_RXIE (1UL << 2)
#define I2C_CR1_STOPIE (1UL << 5)
#define I2C_CR1_TCIE (1UL << 6)
#define I2C_CR1_ERRIE (1UL << 7)

#define I2C_CR2_SADD_MASK 0x3FFUL
#define I2C_CR2_RD_WRN (1UL << 10)
#define I2C_CR2_ADD10 (1UL << 11)
#define I2C_CR2_START (1UL << 13)
#define I2C_CR2_STOP (1UL << 14)
#define I2C_CR2_NBYTES_SHIFT 16U
#define I2C_CR2_NBYTES_MASK (0xFFUL << I2C_CR2_NBYTES_SHIFT)
#define I2C_CR2_RELOAD (1UL << 24)
#define I2C_CR2_AUTOEND (1UL << 25)

#define I2C_ISR_TXIS (1UL << 1)
#define I2C_ISR_RXNE (1UL << 2)
#define I2C_ISR_NACKF (1UL << 4)
#define I2C_ISR_STOPF (1UL << 5)
#define I2C_ISR_TC (1UL << 6)
#define I2C_ISR_BERR (1UL << 8)
#define I2C_ISR_ARLO (1UL << 9)
#define I2C_ISR_OVR (1UL << 10)
#define I2C_ISR_BUSY (1UL << 15)

#define I2C_ICR_NACKCF (1UL << 4)
#define I2C_ICR_STOPCF (1UL << 5)
#define I2C_ICR_BERRCF (1UL << 8)
#define I2C_ICR_ARLOCF (1UL << 9)
#define I2C_ICR_OVRCF (1UL << 10)

#define I2C_ISR_ERROR_MASK (I2C_ISR_BERR | I2C_ISR_ARLO | I2C_ISR_OVR)

#define I2C_TRANSFER_MAX_NBYTES 255U
#define I2C_POLL_RETRIES 10000U

#define I2C_TIMINGR_SM_100K 0x3090216CUL
#define I2C_TIMINGR_FM_400K 0x10700B1CUL
#define I2C_TIMINGR_FMP_1M 0x00602173UL


/* declare my own implementation of standard i2c fops */
static struct i2c_bus_fops my_i2c_specific_fops = {
 	   .write7 = NULL,
	   .read7 = NULL,
	   .write10 = NULL,
	   .read10 = NULL,
};

/* declare my own implementation of standard i2c driver as a platform driver */
static struct platform_device_driver my_i2c_driver = {
 	.label = 0x0UL, /*< to be updated by driver probe */
    .devh = 0, // to be updated by driver probe
	.name = "my I2C bus controller driver for STM32U5A5",
	.compatible = "st,stm32u5-i2c", /*< maybe a table instead, like Linux */
 	.driver_fops = (void*)(&my_i2c_specific_fops),
	/* platform deiver generic fops, not I2C specific */
    .platform_fops = {
        .isr = NULL,
 	},
	.type = DEVICE_TYPE_I2C,
};

#define GET_REG_ADDR(offset) ((my_i2c_driver.devinfo->baseaddr) + (offset))

/** local utility I2C helper functions */
static enum i2c_address_mode stm32_i2c_addr_mode = I2C_ADDRESS_7B;

static void stm32_i2c_bus_disable(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val &= ~I2C_CR1_PE;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void stm32_i2c_bus_enable(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val |= I2C_CR1_PE;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void stm32_i2c_bus_send_stop(void)
{
	uint32_t cr2_val = merlin_ioread32(GET_REG_ADDR(I2C_CR2_OFFSET));
	cr2_val |= I2C_CR2_STOP;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR2_OFFSET), cr2_val);
}

static void stm32_i2c_bus_set_timing(uint32_t timingr)
{
	merlin_iowrite32(GET_REG_ADDR(I2C_TIMINGR_OFFSET), timingr);
}

static void stm32_i2c_bus_clear_flags(uint32_t flags)
{
	merlin_iowrite32(GET_REG_ADDR(I2C_ICR_OFFSET), flags);
}

static int stm32_i2c_bus_wait_flag_set(uint32_t flag)
{
	if (merlin_iopoll32_until_set(GET_REG_ADDR(I2C_ISR_OFFSET), flag, I2C_POLL_RETRIES) != STATUS_OK) {
		return -1;
	}
	return 0;
}

static int stm32_i2c_bus_wait_until_idle(void)
{
	if (merlin_iopoll32_until_clear(GET_REG_ADDR(I2C_ISR_OFFSET), I2C_ISR_BUSY, I2C_POLL_RETRIES) != STATUS_OK) {
		return -1;
	}
	return 0;
}

static int stm32_i2c_bus_check_and_clear_errors(void)
{
	uint32_t isr_val = merlin_ioread32(GET_REG_ADDR(I2C_ISR_OFFSET));

	if ((isr_val & I2C_ISR_NACKF) != 0UL) {
		stm32_i2c_bus_clear_flags(I2C_ICR_NACKCF);
		return -1;
	}

	if ((isr_val & I2C_ISR_ERROR_MASK) != 0UL) {
		stm32_i2c_bus_clear_flags(I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF);
		return -1;
	}

	return 0;
}

static int stm32_i2c_bus_wait_for_stop(void)
{
	if (stm32_i2c_bus_wait_flag_set(I2C_ISR_STOPF) != 0) {
		return -1;
	}

	stm32_i2c_bus_clear_flags(I2C_ICR_STOPCF | I2C_ICR_NACKCF | I2C_ICR_BERRCF | I2C_ICR_ARLOCF | I2C_ICR_OVRCF);
	return 0;
}

static void stm32_i2c_bus_abort_transfer(void)
{
	stm32_i2c_bus_send_stop();
	(void)stm32_i2c_bus_wait_for_stop();
}

static int stm32_i2c_bus_set_addressing_mode(enum i2c_address_mode mode)
{
	if (mode != I2C_ADDRESS_7B && mode != I2C_ADDRESS_10B) {
		return -1;
	}

	stm32_i2c_addr_mode = mode;
	return 0;
}

static int stm32_i2c_bus_configure_transfer(uint16_t slave_addr, size_t nbytes,
	int is_read, int autoend, enum i2c_address_mode mode)
{
	uint32_t cr2_val;

	if (unlikely(nbytes == 0U || nbytes > I2C_TRANSFER_MAX_NBYTES)) {
		return -1;
	}

	cr2_val = merlin_ioread32(GET_REG_ADDR(I2C_CR2_OFFSET));
	cr2_val &= ~(I2C_CR2_SADD_MASK | I2C_CR2_RD_WRN | I2C_CR2_ADD10 |
			I2C_CR2_NBYTES_MASK | I2C_CR2_RELOAD | I2C_CR2_AUTOEND |
			I2C_CR2_START | I2C_CR2_STOP);

	if (mode == I2C_ADDRESS_10B) {
		cr2_val |= ((uint32_t)slave_addr & I2C_CR2_SADD_MASK) | I2C_CR2_ADD10;
	} else {
		cr2_val |= (((uint32_t)slave_addr & 0x7FUL) << 1U);
	}

	cr2_val |= (((uint32_t)nbytes << I2C_CR2_NBYTES_SHIFT) & I2C_CR2_NBYTES_MASK);

	if (is_read != 0) {
		cr2_val |= I2C_CR2_RD_WRN;
	}
	if (autoend != 0) {
		cr2_val |= I2C_CR2_AUTOEND;
	}

	cr2_val |= I2C_CR2_START;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR2_OFFSET), cr2_val);

	return 0;
}

static int stm32_i2c_bus_write_internal(uint16_t slave_addr, uint8_t reg_addr,
	uint8_t *data, size_t length, enum i2c_address_mode mode)
{
	int res = -1;

	if (unlikely(length == 0U || length > (I2C_TRANSFER_MAX_NBYTES - 1U))) {
		goto end;
	}

	if (stm32_i2c_bus_wait_until_idle() != 0) {
		goto end;
	}

	if (stm32_i2c_bus_configure_transfer(slave_addr, length + 1U, 0, 1, mode) != 0) {
		goto end;
	}

	if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
		goto end;
	}
	merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), reg_addr);

	for (size_t idx = 0U; idx < length; idx++) {
		if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
			goto end;
		}
		merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), data[idx]);
	}

	if (stm32_i2c_bus_wait_for_stop() != 0) {
		goto end;
	}

	res = 0;
end:
	if (res != 0) {
		stm32_i2c_bus_abort_transfer();
	}
	return res;
}

static void stm32_i2c_enable_interrupt(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val |= (I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE);
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void stm32_i2c_disable_interrupt(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val &= ~(I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_STOPIE | I2C_CR1_TCIE | I2C_CR1_ERRIE);
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

/* NOTE: in that example driver, DMA-based I2C transfer is not supported */

static int stm32_i2c_bus_write7(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length)
{
	if (stm32_i2c_addr_mode != I2C_ADDRESS_7B) {
		return -1;
	}

	return stm32_i2c_bus_write_internal(slave_addr, reg_addr, data, length, I2C_ADDRESS_7B);
}

static int stm32_i2c_bus_write10(uint16_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length)
{
	if (stm32_i2c_addr_mode != I2C_ADDRESS_10B) {
		return -1;
	}

	return stm32_i2c_bus_write_internal(slave_addr, reg_addr, data, length, I2C_ADDRESS_10B);
}

static int stm32_i2c_bus_read7(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length)
{
	int res = -1;

	if (stm32_i2c_addr_mode != I2C_ADDRESS_7B) {
		goto end;
	}

	if (unlikely(length == 0U || length > I2C_TRANSFER_MAX_NBYTES)) {
		goto end;
	}

	if (stm32_i2c_bus_wait_until_idle() != 0) {
		goto end;
	}

	if (stm32_i2c_bus_configure_transfer(slave_addr, 1U, 0, 0, I2C_ADDRESS_7B) != 0) {
		goto end;
	}

	if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
		goto end;
	}
	merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), reg_addr);

	if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TC) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
		goto end;
	}

	if (stm32_i2c_bus_configure_transfer(slave_addr, length, 1, 1, I2C_ADDRESS_7B) != 0) {
		goto end;
	}

	for (size_t idx = 0U; idx < length; idx++) {
		uint8_t received_data;

		if (stm32_i2c_bus_wait_flag_set(I2C_ISR_RXNE) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
			goto end;
		}
		received_data = merlin_ioread8(GET_REG_ADDR(I2C_RXDR_OFFSET));
		if (data != NULL) {
			data[idx] = received_data;
		}
	}

	if (stm32_i2c_bus_wait_for_stop() != 0) {
		goto end;
	}

	res = 0;
end:
	if (res != 0) {
		stm32_i2c_bus_abort_transfer();
	}
	return res;
}

int stm32_i2c_bus_read10(uint16_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length)
{
	int res = -1;

	if (stm32_i2c_addr_mode != I2C_ADDRESS_10B) {
		goto end;
	}

	/* note that we allow reading to trash (data buffer can be NULL) */
	if (unlikely(length == 0U || length > I2C_TRANSFER_MAX_NBYTES)) {
		goto end;
	}

	if (stm32_i2c_bus_wait_until_idle() != 0) {
		goto end;
	}

	if (stm32_i2c_bus_configure_transfer(slave_addr, 1U, 0, 0, I2C_ADDRESS_10B) != 0) {
		goto end;
	}

	if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TXIS) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
		goto end;
	}
	merlin_iowrite8(GET_REG_ADDR(I2C_TXDR_OFFSET), reg_addr);

	if (stm32_i2c_bus_wait_flag_set(I2C_ISR_TC) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
		goto end;
	}

	if (stm32_i2c_bus_configure_transfer(slave_addr, length, 1, 1, I2C_ADDRESS_10B) != 0) {
		goto end;
	}

	for (size_t idx = 0U; idx < length; idx++) {
		uint8_t received_data;

		if (stm32_i2c_bus_wait_flag_set(I2C_ISR_RXNE) != 0 || stm32_i2c_bus_check_and_clear_errors() != 0) {
			goto end;
		}
		received_data = merlin_ioread8(GET_REG_ADDR(I2C_RXDR_OFFSET));
		if (data != NULL) {
	    	data[idx] = received_data;
		}
	}

	if (stm32_i2c_bus_wait_for_stop() != 0) {
		goto end;
	}

	res = 0;
end:
	if (res != 0) {
		stm32_i2c_bus_abort_transfer();
	}
	return res;
}

static void stm32_i2c_bus_acknowledge_irq(void)
{
	uint32_t isr_val = merlin_ioread32(GET_REG_ADDR(I2C_ISR_OFFSET));
	uint32_t clear_flags = 0;

	if ((isr_val & I2C_ISR_STOPF) != 0UL) {
		clear_flags |= I2C_ICR_STOPCF;
	}
	if ((isr_val & I2C_ISR_NACKF) != 0UL) {
		clear_flags |= I2C_ICR_NACKCF;
	}
	if ((isr_val & I2C_ISR_BERR) != 0UL) {
		clear_flags |= I2C_ICR_BERRCF;
	}
	if ((isr_val & I2C_ISR_ARLO) != 0UL) {
		clear_flags |= I2C_ICR_ARLOCF;
	}
	if ((isr_val & I2C_ISR_OVR) != 0UL) {
		clear_flags |= I2C_ICR_OVRCF;
	}

	if (clear_flags != 0U) {
		stm32_i2c_bus_clear_flags(clear_flags);
	}
}


/* I2C driver exported API implementations */

/**
 * \brief probe routine for my i2c driver, which will be called by the hosting application
 *
 * This probe routine call merlin platform_driver_register() to register the driver in the platform,
 * verify that the corresponding device exists and is owned by the current task, and fulfill
 * all not yet configured fields of my driver declaration based on merlin platform driver API and DTS backend.
 *
 * \return 0 if the driver is successfully registered, or a negative error code otherwise.
 */
static int stm32_i2c_driver_probe(uint32_t label)
{
	/* probing using merlin to check that this driver compatible i2c device exists and is owned */
	if (merlin_platform_driver_register(&my_i2c_driver, label) != STATUS_OK) {
		return -1;
	}
	return 0;
}

/**
 * \brief init routine for my i2c driver, which will be called by the hosting application
 * This routine initialize the I2C bus controller so that it is ready to execute I2C transactions.
 * This routine is called after the probe routine.
 */
static int stm32_i2c_driver_init(enum i2c_speeds speed, enum i2c_address_mode mode)
{
	int res = -1;
	uint32_t timingr;

	if (my_i2c_driver.devh == 0) {
		goto end;
	}
	/* map de I2C controller in the application memory */
	if (merlin_platform_driver_map(&my_i2c_driver) != STATUS_OK) {
		goto end;
	}

	/* configure GPIOs for SCL and SDA lines */
	if (merlin_platform_driver_configure_gpio(&my_i2c_driver) != STATUS_OK) {
		goto end;
	}
	/* configure address mode */
	if (stm32_i2c_bus_set_addressing_mode(mode) != 0) {
		goto end;
	}

	/* configure timing register for target bus speed */
	/** TODO: select timing value dynamically from kernel clock reported by DTS. */
	switch (speed) {
		case I2C_SPEED_SM_100K:
			timingr = I2C_TIMINGR_SM_100K;
			break;
		case I2C_SPEED_FM_400K:
			timingr = I2C_TIMINGR_FM_400K;
			break;
		case I2C_SPEED_FMP_1M:
			timingr = I2C_TIMINGR_FMP_1M;
			break;
		default:
			goto end;
	}

	stm32_i2c_bus_disable();
	stm32_i2c_bus_set_timing(timingr);
	/* enable the I2C bus */
	stm32_i2c_bus_enable();
	/* I2C initialization complete */
	res = 0;
end:
	return res;
}

static int stm32_i2c_driver_release(void)
{
	int res = -1;
	if (my_i2c_driver.devh == 0) {
		goto end;
	}

	stm32_i2c_bus_disable();
	stm32_i2c_disable_interrupt();

	/* unmap de I2C controller from the application memory */
	if (merlin_platform_driver_unmap(&my_i2c_driver) != STATUS_OK) {
		goto end;
	}

	res = 0;
end:
	return res;
}

static void stm32_i2c_isr(uint32_t IRQn)
{
	/** TODO: TBD */

}

/* Aliases for the exported API functions */
int i2c_driver_probe(uint32_t label) __attribute__((alias("stm32_i2c_driver_probe")));
int i2c_driver_init(enum i2c_speeds speed, enum i2c_address_mode mode) __attribute__((alias("stm32_i2c_driver_init")));
int i2c_bus_write7(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length) __attribute__((alias("stm32_i2c_bus_write7")));
int i2c_bus_write10(uint16_t slave_addr, uint8_t reg_addr,
	uint8_t *data, size_t length) __attribute__((alias("stm32_i2c_bus_write10")));
int i2c_bus_read7(uint8_t slave_addr, uint8_t reg_addr,
	uint8_t *data, size_t length) __attribute__((alias("stm32_i2c_bus_read7")));
int i2c_bus_read10(uint16_t slave_addr, uint8_t reg_addr,
	uint8_t *data, size_t length) __attribute__((alias("stm32_i2c_bus_read10")));
void i2c_bus_acknowledge_irq(void) __attribute__((alias("stm32_i2c_bus_acknowledge_irq")));
int i2c_driver_release(void) __attribute__((alias("stm32_i2c_driver_release")));
