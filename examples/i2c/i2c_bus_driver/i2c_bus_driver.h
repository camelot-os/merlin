// SPDX-License-Identifier: BSD-3-Clause
// Copyright (c) 2026 H2Lab Development Team

#ifndef MY_I2C_BUS_DRIVER_H
#define MY_I2C_BUS_DRIVER_H

#include <inttypes.h>
#include <merlin/buses/i2c.h>

/*
 * I2C message flags. This is a bitfield to allow ORing multiple flags.
 * By now, the first bit specify the message direction (RD or WR)
 */
typedef enum i2c_msg_flags {
    I2C_MSG_FLAG_WR      = 0x1, /*< write data */
    I2C_MSG_FLAG_RD      = 0x2, /*< read data */
    I2C_MSG_FLAG_RCV_LEN = 0x4  /*< data read len is received in first byte */
} i2c_msg_flags_t;

/*
 * I2C message metadata
 *
 * When sending or receiving an I2C message, this struct is used to handle both
 * the message metadata (slave address, type of I2C communication (read, write,
 * dynamic len or not)) and the message buffer.
 */
typedef struct i2c_msg {
    uint8_t         addr;  /*< target slave address (7bits addr only) */
    i2c_msg_flags_t flags; /*< transfer mode */
    uint16_t        len; /*< bytes to read, if I2C_MSG_FLAG_RCV_LEN, buf max size */
    uint8_t*        buf; /*< buffer to emit (WR)/to fulfill (RD) */
} i2c_msg_t;


/**
 * @brief probe routine for my i2c driver, which will be called by the hosting application
 *
 * The device is probed based on the DTS label which uniquely identify it.
 * This is the lonely application level information that need to be provided,
 * all other information (base address, IRQs, GPIOs and so on) are retrieved from
 * merlin framework.
 * Note that this function do not map, neither initialize the device, but only register
 * it in the platform. mapping is done in the init routine, which is called after the probe routine.
 *
 * caution: note that this example driver handle only one I2C bus controller, and thus the probe
 * function do not check the label value. This can be easily adaptet by adding a table instead of a
 * single driver platform_device_driver structure, and by passing to all below functions
 * an additional argument to specify on which I2C bus controller the transaction should be executed.
 *
 * @param label device label as set in the dts using sentry,label attribute
 */
int i2c_driver_probe(uint32_t label);

/**
 * @brief initialize the I2C bus c ontroller.
 *
 * This routine map the device in the application memory, and configure the
 * i2c bus controller to be ready to execute I2C transactions.
 * This also include the GPIO configuration for SCL and SDA lines to be set.
 *
 * @param speed I2C speed mode to be configured, based on the enum i2c_speeds definition
 * @param mode I2C addressing mode to be configured, based on the enum i2c_address_mode definition
 *
 * @return 0 if initialization is successful, or -1 if any error occur
 */
int i2c_driver_init(enum i2c_speeds speed, enum i2c_address_mode mode);

/**
 * @brief write data to a slave device on the I2C bus, using 7bits addressing mode
 *
 * This function execute a complete I2C transaction, including start condition, slave address,
 * register address, data and stop condition.
 * The function wait for each step of the transaction to be completed before going to the next one,
 * and return an error if any step fail or if the transaction is not compliant with I2C
 * specifications (for example, if the length is 0 or greater than 255 bytes, which is the maximum
 * supported by the I2C controller in that example).
 *
 * @param slave_addr 7bits slave address of the target device
 * @param reg_addr register address to write to
 * @param data buffer containing the data to be written
 * @param length number of bytes to write, must be between 1 and 255
 */
int i2c_bus_read7(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

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
 * @param slave_addr 7bits slave address of the target device
 * @param reg_addr register address to write to
 * @param data buffer containing the data to be written
 * @param length number of bytes to write, must be between 1 and 255
 */
int i2c_bus_write7(uint8_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

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
 * @param slave_addr 10bits slave address of the target device
 * @param reg_addr register address to read from
 * @param data buffer to be filled with the data read from the slave device
 * @param length number of bytes to read, must be between 1 and 255
 */
int i2c_bus_read10(uint16_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

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
 * @param slave_addr 10bits slave address of the target device
 * @param reg_addr register address to write to
 * @param data buffer containing the data to be written
 * @param length number of bytes to write, must be between 1 and 255
 */
int i2c_bus_write10(uint16_t slave_addr, uint8_t reg_addr, uint8_t *data, size_t length);

/**
 * @brief acknowledge an I2C bus controller interrupt
 *
 * This function is used to acknowledge an interrupt from the I2C bus controller, by reading the appropriate
 * status registers. This is typically called in the I2C bus interrupt service routine, to
 * acknowledge the interrupt and allow the controller to process the next events.
 *
 * This IRQ is linked to the controller itself, not to a given I2C device that hold its own, dedicated
 * IRQ line through usual GPIO-based interrupt.
 *
 * @see the ili2130_driver.c example for a typical device IRQ handler that call this function to acknowledge
 * the I2C bus controller IRQ, and then process the device-specific events.
 */
void i2c_bus_acknowledge_irq(uint32_t IRQn);

/**
 * @brief release the I2C bus controller.
 *
 * This routine unmap the device from the application memory, and do any other
 * clean up operation if needed.
 */
int i2c_driver_release(void);

#endif/* MY_I2C_BUS_DRIVER_H */
