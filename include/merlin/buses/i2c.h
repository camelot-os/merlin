// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_I2C_H
#define MERLIN_I2C_H

#include <types.h>
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

#endif/*!MERLIN_I2C_H*/
