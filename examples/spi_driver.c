/*
 * a typical SPI bus driver then needs only the following in its source code:
 */
#include <merlin/buses/spi.h>

#define MY_SPI_BUS_LABEL CONFIG_SPI_ST32_LABEL

static int my_driver_xfer(struct platform_device_driver *self, uint8_t *rdbuf, uint8_t *wrbuf, size_t len) {
 	// [...]
}

static int my_driver_probe(struct platform_device_driver *self, u32 label) {
 	self->devh = sys_get_handle(label)
 	// [...]
 }

static int my_driver_init(struct platform_device_driver *self) {
 	merlin_platform_map(self->devh);
 	// initialize SPI bus
 	// [...]
 	// configure interrupts
 	for (uint32_t irq = merlin_platform_interrupt_iterate(self)) {
 	    sys_interrupt_enable(irq);
    }
}

static int my_driver_release(struct platform_device_driver *self) {
 	merlin_platform_map(self->devh);
 	// release SPI bus, mask IT, and so on
 	// [...]
 	// disable interrupts
 	for (uint32_t irq = merlin_platform_interrupt_iterate(self)) {
 	    sys_interrupt_disable(irq);
    }
}

static int my_driver_isr(struct platform_device_driver *self, uint32_t IRQn) {
    // react to IRQ reception that targets my driver
}

static struct spi_bus_fops my_spi_specific_fops {
 	   .xfer = my_driver_xfer,
};

static struct platform_device_driver my_spi_driver {
 	.label = MY_SPI_BUS_LABEL,
    .devh = 0, // to be updated by driver probe
 	.name = "my SPI driver for XXX",
 	.compatible = "st,stm32", /*< maybe a table instead, like Linux */
 	.driver_fops = (void*)(&my_spi_specific_fops),
    .platform_fops = {
        .probe = my_driver_probe,
        .init = my_driver_init,
        .release = my_driver_release,
        .isr = my_driver_isr,
 	}
};
  // natural automatic declaration at task startup, TBD (marcro ?)
merlin_platform_driver(&my_spi_driver);

/*
 * ote: there is no need for declaring each IRQ as owned by the device as
 * the link between IRQn and devh_t is under the private control of Merlin,
 */
