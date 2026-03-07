#ifndef MY_I2C_BUS_DRIVER_H
#define MY_I2C_BUS_DRIVER_H

#include <inttypes.h>

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
 * I2C speed modes.
 */
enum i2c_speeds {
	I2C_SPEED_SM_100K,
	I2C_SPEED_FM_400K,
	I2C_SPEED_FMP_1M,
	I2C_SPEED_UNKNOWN
};

enum i2c_address_modes {
    I2C_ADDRESS_7B,
    I2C_ADDRESS_10B,
};


/**
 * \brief probe routine for my i2c driver, which will be called by the hosting application
 *
 * The device is probed based on the DTS label which uniquely identify it.
 * This is the lonely application level information that need to be provided,
 * all other information (base address, IRQs, GPIOs and so on) are retrieved from
 * merlin framework.
 * Note that this function do not map, neither initialize the device, but only register
 * it in the platform.
 */
int i2c_driver_probe(uint32_t label);

/**
 * \brief initialize the I2C bus c ontroller.
 * This routine map the device in the application memory, and configure the
 * i2c bus controller to be ready to execute I2C transactions.
 * This also include the GPIO configuration for SCL and SDA lines to be set.
 */
int i2c_driver_init(enum i2c_speeds speed, enum i2c_address_modes mode);

/**
 * \brief release the I2C bus controller.
 * This routine unmap the device from the application memory, and do any other
 * clean up operation if needed.
 */
int i2c_driver_release(void);

#endif/* MY_I2C_BUS_DRIVER_H */
