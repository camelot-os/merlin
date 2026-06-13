// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_API_I2C_H
#define MERLIN_PLATFORM_API_I2C_H

#include <inttypes.h>
#include <stddef.h>
#include <merlin/platform/api/generic.h>
/* I2C bus address mode and speed definitions are a part of upper layer exported infos */
#include <merlin/buses/i2c.h>

/**
 * @brief probe routine for I2C drivers, which will be called by the hosting application
 *
 * The device is probed based on the DTS label which uniquely identify it.
 * This is the lonely application level information that need to be provided,
 * all other information (base address, IRQs, GPIOs and so on) are retrieved from
 * merlin framework.
 * Note that this function do not map, neither initialize the device, but only register
 * it in the platform. mapping is done in the init routine, which is called after the probe routine.
 *
 *
 * @param label device label as set in the dts using sentry,label attribute
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_NOTREGISTERED if the merlin registration step failed
 * @return DRV_ERROR_INVSTATE if the driver instance is already probed
 * @return DRV_ERROR_CONFIGURATION if the device is not found in the DTS
 */
drv_status_t i2c_probe(uint32_t label);

/**
 * @brief initialize the I2C bus controller.
 *
 * This routine map the device in the application memory, and configure the
 * i2c bus controller to be ready to execute I2C transactions.
 * This also include the GPIO configuration for SCL and SDA lines to be set.
 *
 * @param label device label as set in the dts using sentry,label attribute
 * @param speed I2C speed mode to be configured, based on the enum i2c_speeds definition
 * @param mode I2C addressing mode to be configured, based on the enum i2c_address_mode definition
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not probed yet
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_CONFIGURATION if the device configuration failed
 */
drv_status_t i2c_init(uint32_t label, enum i2c_speeds speed, enum i2c_address_mode mode);

/**
 * @brief read data from a slave device on the I2C bus, using 7bits addressing mode
 *
 * This function execute a complete I2C transaction, including start condition, slave address,
 * register address, data and stop condition.
 * The function wait for each step of the transaction to be completed before going to the next one,
 * and return an error if any step fail or if the transaction is not compliant with I2C
 * specifications (for example, if the length is 0 or greater than 255 bytes, which is the maximum
 * supported by the I2C controller in that example).
 *
 * @param label device label as set in the dts using sentry,label attribute
 * @param slave_addr 7bits slave address of the target device
 * @param reg_addr register address to read from
 * @param data buffer to be filled with received bytes (can be NULL to discard)
 * @param length number of bytes to read, must be between 1 and 255
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised yet
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_AGAIN if the transaction should be retried
 */
drv_status_t i2c_read7(uint32_t label, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

/**
 * @brief write data to a slave device on the I2C bus, using 7bits addressing mode
 *
 * This function execute a complete I2C transaction, including start condition, slave address,
 * register address, data and stop condition.
 * The function wait for each step of the transaction to be completed before going to the next one
 * and return an error if any step fail or if the transaction is not compliant with I2C
 * specifications (for example, if the length is 0 or greater than 255 bytes, which is the maximum
 * supported by the I2C controller in that example).
 * Note that the data buffer must be properly filled with the data to be written before calling the function.
 *
 * @param label device label as set in the dts using sentry,label attribute
 * @param slave_addr 7bits slave address of the target device
 * @param reg_addr register address to write to
 * @param data buffer containing the data to be written
 * @param length number of bytes to write, must be between 1 and 255
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised yet
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_AGAIN if the transaction should be retried
 */
drv_status_t i2c_write7(uint32_t label, uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

/**
 * @brief read data from a slave device on the I2C bus, using 10bits addressing mode
 *
 * This function execute a complete I2C transaction, including start condition, slave address,
 * register address, data and stop condition.
 * The function wait for each step of the transaction to be completed before going to the next one
 * and return an error if any step fail or if the transaction is not compliant with I2C
 * specifications (for example, if the length is 0 or greater than 255 bytes, which is the maximum
 * supported by the I2C controller in that example).
 * Note that the data buffer must be properly allocated before calling the function, and the function
 * will fill it with the data read from the slave device.
 *
 * @param label device label as set in the dts using sentry,label attribute
 * @param slave_addr 10bits slave address of the target device
 * @param reg_addr register address to read from
 * @param data buffer to be filled with the data read from the slave device
 * @param length number of bytes to read, must be between 1 and 255
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised yet
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_AGAIN if the transaction should be retried
 * @return DRV_ERROR_UNSUPPORTED if the device does not support 10bits addressing mode
 */
drv_status_t i2c_read10(uint32_t label, uint16_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

/**
 * @brief write data to a slave device on the I2C bus, using 10bits addressing mode
 *
 * This function execute a complete I2C transaction, including start condition, slave address,
 * register address, data and stop condition.
 * The function wait for each step of the transaction to be completed before going to the next one
 * and return an error if any step fail or if the transaction is not compliant with I2C
 * specifications (for example, if the length is 0 or greater than 255 bytes, which is the maximum
 * supported by the I2C controller in that example).
 * Note that the data buffer must be properly filled with the data to be written before calling the function.
 *
 * @param label device label as set in the dts using sentry,label attribute
 * @param slave_addr 10bits slave address of the target device
 * @param reg_addr register address to write to
 * @param data buffer containing the data to be written
 * @param length number of bytes to write, must be between 1 and 255
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised yet
 * @return DRV_ERROR_INVPARAM if the provided parameters are invalid
 * @return DRV_ERROR_AGAIN if the transaction should be retried
 * @return DRV_ERROR_UNSUPPORTED if the device does not support 10bits addressing mode
 */
drv_status_t i2c_write10(uint32_t label, uint16_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

/**
 * @brief release the I2C bus controller.
 *
 * This routine unmap the device from the application memory, and do any other
 * clean up operation if needed.
 *
 * @param label device label as set in the dts using sentry,label attribute
 *
 * @return DRV_STATUS_OK on success
 * @return DRV_ERROR_NOTREGISTERED if the driver instance is not probed or not found
 * @return DRV_ERROR_INVSTATE if the driver instance is not initialised yet
 */
drv_status_t i2c_release(uint32_t label);

#endif /* MERLIN_PLATFORM_API_I2C_H */
