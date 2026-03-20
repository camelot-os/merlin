// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_I2C_H
#define MERLIN_I2C_H

#include <types.h>
#include <merlin/platform/driver.h>


/**
 * @brief I2C speed modes, as defined by the I2C specifications.
 *
 * This is used to configure the I2C bus controller to operate at the desired speed.
 * The actual timing values for each speed mode are specific to the I2C bus controller
 * and need to be defined in the driver implementation.
 */
enum i2c_speeds {
	I2C_SPEED_SM_100K,   /**< Standard Mode (100 kHz) */
	I2C_SPEED_FM_400K,   /**< Fast Mode (400 kHz) */
	I2C_SPEED_FMP_1M,    /**< Fast Mode Plus (1 MHz) */
	I2C_SPEED_HS_1M,     /**< High Speed (1.7 MHz) */
	I2C_SPEED_HS_3M,     /**< High Speed (3.4 MHz) */
	I2C_SPEED_UFM_5M,    /**< Ultra Fast Mode (5 MHz) */
	I2C_SPEED_UNKNOWN
};

/**
 * @brief I2C addressing modes, as defined by the I2C specifications
 *
 * This is used to configure the I2C bus controller to operate with the desired addressing mode.
 * The actual handling of the addressing mode is specific to the I2C bus controller and needs
 * to be implemented in the driver implementation. Note that some I2C bus controller may support
 * only one addressing mode, in that case the driver implementation should check the mode and return
 * an error if the mode is not supported.
 */
enum i2c_address_mode {
    I2C_ADDRESS_7B,    /**< 7-bit addressing mode */
    I2C_ADDRESS_10B,   /**< 10-bit addressing mode */
};

/** I2C driver structure pre-declaration */
struct i2c_driver;

/* I2C specific functions that need to be implemented by the driver */

/**
 * @brief I2C write function for 7-bit register address
 *
 * @param self pointer to the i2c_driver structure, which contains the private data and the fops of the driver
 * @param wdbuf buffer containing the data to be written, including the slave address
 * @param len number of bytes to write
 *
 * @return 0 on success, negative error code on failure
 */
typedef int	(*i2c_write_reg7_fn_t)(struct i2c_driver *self, uint8_t *wdbuf, size_t len);

/**
 * @brief I2C read function for 7-bit register address
 *
 * @param self pointer to the i2c_driver structure, which contains the private data and the fops of the driver
 * @param rdbuf buffer to store the read data, including the slave address
 * @param len number of bytes to read
 *
 * @return 0 on success, negative error code on failure
 */
typedef int	(*i2c_read_reg7_fn_t)(struct i2c_driver *self, uint8_t *rdbuf, size_t len);

/**
 * @brief I2C write function for 10-bit register address
 *
 * @param self pointer to the i2c_driver structure, which contains the private data and the fops of the driver
 * @param wdbuf buffer containing the data to be written, including the slave address
 * @param len number of bytes to write
 *
 * @return 0 on success, negative error code on failure
 */
typedef int	(*i2c_write_reg10_fn_t)(struct i2c_driver *self, uint8_t *wrbuf, size_t len);

/**
 * @brief I2C read function for 10-bit register address
 *
 * @param self pointer to the i2c_driver structure, which contains the private data and the fops of the driver
 * @param rdbuf buffer to store the read data, including the slave address
 * @param len number of bytes to read
 *
 * @return 0 on success, negative error code on failure
 */
typedef int	(*i2c_read_reg10_fn_t)(struct i2c_driver *self, uint8_t *rdbuf, size_t len);

/**
 * @brief I2C specific fops definition
 *
 * This function needs to be implemented in the I2C driver and declared
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
