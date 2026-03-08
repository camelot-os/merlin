// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PLATFORM_H
#define MERLIN_PLATFORM_H

#include <types.h>
#include <device.h>
#include <handle.h>
/*
 * generic platform device primitives that need to be defined for
 * all platform devices (Buses typically).
 * Non in-SoC devices are **not** platform devices but uses a given
 * plateform device driver (being a bus device) in order to communicate
 * with such an external device (e.g. touchscreen, panel, battery and so on)
 */

typedef enum device_type {
	DEVICE_TYPE_I2C = 1, /*< I2C bus controller */
	DEVICE_TYPE_SPI = 2, /*< SPI bus controller */
	DEVICE_TYPE_USART = 3, /*< USART bus controller */
	DEVICE_TYPE_CAN = 4, /*< CAN bus controller */
	DEVICE_TYPE_USB = 5, /*< USB bus controller */
	DEVICE_TYPE_GPIO = 6, /*< bare GPIO device (led, button, etc.) */
} device_type_t;

/* initialize the given device */
typedef int (*merlin_platform_init_fn_t)(void *self);
/* release the given device */
typedef int (*merlin_platform_release_fn_t)(void *self);
/* ISR triggered when an IRQn associated to the device is received */
typedef int (*merlin_platform_isr_fn_t)(void *self, uint32_t IRQn);

struct platform_fops {
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
	const devinfo_t *devinfo; /**< device information retrieved from DTS, based on the label */
	/**< device name for debug purpose */
	const char * name;
	/**< device compatible field declared in the bus driver, used at probe time */
	const char * compatible;
	/**< per driver-type fops vary depending on the driver family */
	void * driver_fops;
	/**< generic platform operations, common to all platform drivers */
	struct platform_fops platform_fops;
	device_type_t type;
};

/* platform level utility functions, that do not need driver-level implementation */

/* probe for the existence and ownership of the given device handle */
/**
 * Note: because self type varies depending on the driver family, the probe function is
 * defined as a generic one that only take the device label as argument, and return the
 * device handle if the device is found and owned by the current task, or 0 otherwise.
 * For each device type, this prototype definition override the void* using the corresponding
 * device-specific structure.
 */
Status merlin_platform_driver_register(struct platform_device_driver *driver, uint32_t label);

/* Map given device handle into the task memory */
Status merlin_platform_driver_map(struct platform_device_driver *self);

/* Unmap given device handle from the task memory */
Status merlin_platform_driver_unmap(struct platform_device_driver *self);

/* resolve driver context in the registered driver(s) for given IRQn using DTS backend,
 * and call the fops->isr routine for it
 */
Status merlin_platform_driver_irq_displatch(uint32_t IRQn);

Status merlin_platform_driver_configure_gpio(struct platform_device_driver *self);

#endif/*!MERLIN_PLATFORM_H*/
