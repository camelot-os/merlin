#ifndef MERLIN_SPI_H
#define MERLIN_SPI_H

#include <types.h>
#include <merlin/platform/driver.h>

struct spi_driver;

typedef int	(*spi_xfer_fn_t)(struct spi_driver *self, uint8_t *rdbuf, uint8_t *wrbuf, size_t len);

/*
 * SPI specific fops, which is, for SPI, only xfer(). This function
 * needs to be implemented in the SPI driver and declared
 */
struct spi_bus_fops {
	spi_xfer_fn_t xfer;
};

/* structure that declare a SPI Bus driver, which is a platform driver */
struct spi_driver {
	/**< SPI sprcific fops, see above */
	struct spi_bus_fops * fops;
	/**< platform-generic fops */
	struct platform_device * platform_fops;
	void *private_data;
};


#endif/*!MERLIN_SPI_H*/
