#ifndef MERLIN_PLATFORM_H
#define MERLIN_PLATFORM_H

#include <types.h>
/*
 * generic platform device primitives that need to be defined for
 * all platform devices (Buses typically).
 * Non in-SoC devices are **not** platform devices but uses a given
 * plateform device driver (being a bus device) in order to communicate
 * with such an external device (e.g. touchscreen, panel, battery and so on)
 */

/* probe for the existence and ownership of the given device handle */
/**
 * Note: because self type varies depending on the driver family, the probe function is
 * defined as a generic one that only take the device label as argument, and return the
 * device handle if the device is found and owned by the current task, or 0 otherwise.
 * For each device type, this prototype definition override the void* using the corresponding
 * device-specific structure.
 */
typedef int (*merlin_platform_probe_fn_t)(uint32_t label);
/* initialize the given device */
typedef int (*merlin_platform_init_fn_t)(void *self);
/* release the given device */
typedef int (*merlin_platform_release_fn_t)(void *self);
/* ISR triggered when an IRQn associated to the device is received */
typedef int (*merlin_platform_isr_fn_t)(void *self, uint32_t IRQn);

struct platform_fops {
	merlin_platform_probe_fn_t    probe;
	merlin_platform_init_fn_t     init;
	merlin_platform_release_fn_t  release;
	merlin_platform_isr_fn_t	  isr;
};

/*
 * generic to all platform devices FOPS definition
 * This structure aim to be used by various platform device families (i2c, SPI, usart, and so on) as
 * a part of the device declaration
 */

struct platform_device_driver {
	/**< unique device handle associated to the device, forge at boot time */
	devh_t 	  devh;
	uint32_t  label;
	/**< device name for debug purpose */
	const char * name;
	/**< device compatible field declared in the bus driver, used at probe time */
	const char * compatible;
	/**< per driver-type fops vary depending on the driver family */
	void * driver_fops;
	/**< generic platform operations, common to all platform drivers */
	struct platform_fops platform_fops;
};

/* platform level utility functions, that do not need driver-level implementation */

/* Map given device handle into the task memory */
Status merlin_platform_driver_map(struct platform_device_driver *self);

/* Unmap given device handle from the task memory */
Status merlin_platform_driver_unmap(struct platform_device_driver *self);

/* resolve driver context in the registered driver(s) for given IRQn using DTS backend,
 * and call the fops->isr routine for it
 */
Status merlin_platform_driver_irq_displatch(uint32_t IRQn);

#endif/*!MERLIN_PLATFORM_H*/
