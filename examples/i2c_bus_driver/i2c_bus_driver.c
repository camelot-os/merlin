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
#define I2C_DR_OFFSET 0x10
#define I2C_SR1_OFFSET 0x14
#define I2C_SR2_OFFSET 0x18
#define I2C_CCR_OFFSET 0x1C
#define I2C_TRISE_OFFSET 0x20

#define I2C_CR1_PE (1 << 0)
#define I2C_CR1_START (1 << 8)
#define I2C_CR1_STOP (1 << 9)
#define I2C_CR1_ACK (1 << 10)

#define I2C_SR1_SB (1 << 0)
#define I2C_SR1_ADDR (1 << 1)
#define I2C_SR1_BTF (1 << 2)

#define I2C_SR2_MSL (1 << 0)
#define I2C_SR2_BUSY (1 << 1)
#define I2C_SR2_TRA (1 << 2)

#define I2C_CCR_FS (1 << 15)
#define I2C_CCR_DUTY (1 << 14)
#define I2C_CCR_CCR_MASK 0xFFF
#define I2C_TRISE_MAX 0x3F

#define I2C_CR2_FREQ_MASK 0x3F


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
 	.name = "my SPI driver bus controler for STM32",
 	.compatible = "st,stm32", /*< maybe a table instead, like Linux */
 	.driver_fops = (void*)(&my_i2c_specific_fops),
	/* platform deiver generic fops, not I2C specific */
    .platform_fops = {
        .isr = NULL,
 	},
	.type = DEVICE_TYPE_I2C,
};

#define GET_REG_ADDR(offset) ((my_i2c_driver.devinfo->baseaddr) + (offset))

/** local utility I2C helper functions */
static void i2c_bus_disable(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val &= ~I2C_CR1_PE;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void i2c_bus_enable(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val |= I2C_CR1_PE;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void i2c_bus_send_start(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val |= I2C_CR1_START;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void i2c_bus_send_stop(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val |= I2C_CR1_STOP;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void i2c_bus_send_ack(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val |= I2C_CR1_ACK;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void i2c_bus_send_nack(void)
{
	uint32_t cr1_val = merlin_ioread32(GET_REG_ADDR(I2C_CR1_OFFSET));
	cr1_val &= ~I2C_CR1_ACK;
	merlin_iowrite32(GET_REG_ADDR(I2C_CR1_OFFSET), cr1_val);
}

static void i2c_bus_wait_for_sb(void)
{
	/* waiting for sb bit to be set */
	merlin_iopoll32_until_clear(
		GET_REG_ADDR(I2C_SR1_OFFSET),
		I2C_SR1_SB,
		1000
	);
}

void i2c_bus_set_prescaler(uint32_t freq)
{
	uint32_t cr2 = merlin_ioread32(GET_REG_ADDR(I2C_CR2_OFFSET));
	cr2 &= ~I2C_CR2_FREQ_MASK;
	cr2 |= (freq & I2C_CR2_FREQ_MASK);
	merlin_iowrite32(GET_REG_ADDR(I2C_CR2_OFFSET), cr2);
}

/* I2C driver exported API implementations */

/**
 * @brief probe routine for my i2c driver, which will be called by the hosting application
 *
 * This probe routine call merlin platform_driver_register() to register the driver in the platform,
 * verify that the corresponding device exists and is owned by the current task, and fulfill
 * all not yet configured fields of my driver declaration based on merlin platform driver API and DTS backend.
 *
 * @param label device label as set in the dts using sentry,label attribute
 * @return 0 if the driver is successfully registered, or a negative error code otherwise.
 */
static int stm32_i2c_driver_probe(uint32_t label)
{
	if (merlin_platform_driver_register(&my_i2c_driver, label) != STATUS_OK) {
		return -1;
	}
	return 0;
}

/**
 * @brief init routine for my i2c driver, which will be called by the hosting application
 *
 * This routine initialize the I2C bus controller so that it is ready to execute I2C transactions.
 * This routine is called after the probe routine.
 *
 * @param speed   requested I2C bus speed
 * @param mode    addressing mode (7-bit or 10-bit)
 * @return 0 on success, negative error code otherwise
 */
static int stm32_i2c_driver_init(enum i2c_speeds speed, enum i2c_address_modes mode)
{
	int res = -1;
	if (my_i2c_driver.devh == 0) {
		goto end;
	}
	/* map de I2C controller in the application memory */
	if (merlin_platform_driver_map(&my_i2c_driver) != STATUS_OK) {
		goto end;
	}

	/* configure GPIOs for SCL and SDA lines */
	merlin_platform_driver_configure_gpio(&my_i2c_driver);
	/* configure prescaler for needed frequency */
	/** @todo Implement proper frequency calculation based on APB clock.
	 * This should be something that can be get back from the DTS fields, through merlin.
	 */
	switch (speed) {
		case I2C_SPEED_SM_100K:
			i2c_bus_set_prescaler(0x10);
			break;
		case I2C_SPEED_FM_400K:
			i2c_bus_set_prescaler(0x4);
			break;
		case I2C_SPEED_FMP_1M:
			i2c_bus_set_prescaler(0x2);
			break;
		default:
			goto end;
	}
	/* enable the I2C bus */
	i2c_bus_enable();
	/* I2C initialization complete */
end:
	return res;
}

static int stm32_i2c_driver_release(void)
{
	int res = -1;
	if (my_i2c_driver.devh == 0) {
		goto end;
	}
	/* unmap de I2C controller from the application memory */
	if (merlin_platform_driver_unmap(&my_i2c_driver) != STATUS_OK) {
		goto end;
	}
end:
	return res;
}

/* Aliases for the exported API functions */
int i2c_driver_probe(uint32_t label) __attribute__((alias("stm32_i2c_driver_probe")));
int i2c_driver_init(enum i2c_speeds speed, enum i2c_address_modes mode) __attribute__((alias("stm32_i2c_driver_init")));
int i2c_driver_release(void) __attribute__((alias("stm32_i2c_driver_release")));
