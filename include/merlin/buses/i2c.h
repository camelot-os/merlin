// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_I2C_H
#define MERLIN_I2C_H

#include <types.h>
#include <merlin/platform/driver.h>

/** I2C driver structure pre-declaration */
struct i2c_driver;

/* I2C specific functions that need to be implemented by the driver */
typedef int	(*i2c_write_reg7_fn_t)(struct i2c_driver *self, uint8_t *wdbuf, size_t len);
typedef int	(*i2c_read_reg7_fn_t)(struct i2c_driver *self, uint8_t *rdbuf, size_t len);
typedef int	(*i2c_write_reg10_fn_t)(struct i2c_driver *self, uint8_t *wrbuf, size_t len);
typedef int	(*i2c_read_reg10_fn_t)(struct i2c_driver *self, uint8_t *rdbuf, size_t len);

/**
 * @brief I2C specific fops definition
 *
 * This function needs to be implemented in the SPI driver and declared
 */
struct i2c_bus_fops {
	i2c_write_reg7_fn_t write7;    /**< write function for 7-bit register address */
	i2c_read_reg7_fn_t read7;      /**< read function for 7-bit register address */
	i2c_write_reg10_fn_t write10;  /**< write function for 10-bit register address */
	i2c_read_reg10_fn_t read10;    /**< read function for 10-bit register address */
};

/**
 * @brief that declare a I2C Bus driver, which is a type of platform driver
 */
struct i2c_driver {
	struct i2c_bus_fops * fops;             /**< I2C specific fops, see above */
	struct platform_device * platform_fops; /**< platform-generic fops */
	void *private_data;                     /**< private data for the driver, if needed */
};


#endif/*!MERLIN_I2C_H*/
