#ifndef MERLIN_I2C_H
#define MERLIN_I2C_H

#include <types.h>
#include <merlin/platform/driver.h>

struct i2c_driver;

typedef int	(*i2c_xfer_fn_t)(struct i2c_driver *self, uint8_t *rdbuf, uint8_t *wrbuf, size_t len);

/*
 * I2C specific fops, which is, for I2C, only xfer(). This function
 * needs to be implemented in the SPI driver and declared
 */
struct i2c_bus_fops {
	spi_xfer_fn_t xfer;
};

/* structure that declare a I2C Bus driver, which is a platform driver */
struct i2c_driver {
	/**< I2C specific fops, see above */
	struct i2c_bus_fops * fops;
	/**< platform-generic fops */
	struct platform_device * platform_fops;
	void *private_data;
};


#endif/*!MERLIN_I2C_H*/
