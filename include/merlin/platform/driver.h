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
	DEVICE_TYPE_I2C   = 1,   /**< I2C bus controller */
	DEVICE_TYPE_SPI   = 2,   /**< SPI bus controller */
	DEVICE_TYPE_USART = 3,   /**< USART bus controller */
	DEVICE_TYPE_CAN   = 4,   /**< CAN bus controller */
	DEVICE_TYPE_USB   = 5,   /**< USB bus controller */
	DEVICE_TYPE_GPIO  = 6,   /**< bare GPIO device (led, button, etc.) */
} device_type_t;

/* initialize the given device */
typedef int (*merlin_platform_init_fn_t)(void *self);
/* release the given device */
typedef int (*merlin_platform_release_fn_t)(void *self);
/* ISR triggered when an IRQn associated to the device is received */
typedef int (*merlin_platform_isr_fn_t)(void *self, uint32_t IRQn);

/**
 * @brief platform device driver structure definition
 */
struct platform_fops {
	merlin_platform_init_fn_t     init;    /**< initialization function */
	merlin_platform_release_fn_t  release; /**< release function */
	merlin_platform_isr_fn_t	  isr;     /**< interrupt service routine */
};

/**
 * @brief generic to all platform devices FOPS definition
 *
 * This structure aim to be used by various platform device families (i2c, SPI, usart, and so on) as
 * a part of the device declaration
 *
 */
struct platform_device_driver {
	devh_t 	  devh;	 /**< unique device handle associated to the device, forge at boot time */
	uint32_t  label; /**< device label as set in the dts file using sentry,label attribute */
	const devinfo_t *devinfo; /**< device information retrieved from DTS, based on the label */
	const char * name; /**< device name for debug purpose */
	const char * compatible; /**< device compatible field declared in the bus driver, used at probe time */
	void * driver_fops; /**< per driver-type fops vary depending on the driver family */
	struct platform_fops platform_fops; /**< generic platform operations, common to all platform drivers */
	device_type_t type; /**< device type, that allows to discriminates the way merlin interact with the dts backend */
};

/* platform level utility functions, that do not need driver-level implementation */


/**
 * @brief probe for the existence and ownership of the given device handle
 *
 * This function registers the device driver and validate that the corresponding device
 * do exist in the device tree, is owned by the task and update the driver metadata with
 * usefull informations the driver can use afterward.
 *
 * @param driver driver infos to be registered and updated by merlin with backend informations
 * @param label device label as set in the dts using sentry,label attribute
 *
 * @return STATUS_OK if device found and metadata updated, or other status codes depending on the error
 *
 * @note because self type varies depending on the driver family, the probe function is
 * defined as a generic one that only take the device label as argument, and return the
 * device handle if the device is found and owned by the current task, or 0 otherwise.
 * For each device type, this prototype definition override the void* using the corresponding
 * device-specific structure.
 */
Status merlin_platform_driver_register(struct platform_device_driver *driver, uint32_t label);

/**
 * @brief Map given device handle into the task memory
 *
 * This function help with device memory mapping using Senty uapi.
 * This function can be used only after merlin_platform_driver_register was successfully executed
 *
 * @param self device driver metadata
 *
 */
Status merlin_platform_driver_map(struct platform_device_driver *self);

/**
 * @brief Unmap given device handle from the task memory
 *
 * This function help with device memory mapping using Senty uapi.
 * This function can be used only after merlin_platform_driver_register was successfully executed
 * The function can be called when the device is mapped. Otherwise it will return an error.
 *
 * @param self device driver metadata
 *
 */
Status merlin_platform_driver_unmap(struct platform_device_driver *self);

/**
 * @brief this function aim to automatically call the IRQ handler of the various registered drivers
 *
 * This function aim to be used in the wait_for_event() Sentry UAPI when EVENT_INTERRUPT is received.
 * As Merlin knows all device drivers and associated device IRQ identifier, it can properly call the
 * corresponding driver interrupt service routine based on the previously registered drivers metadata.
 *
 * This function allows a task to manipulate multiple device drivers for multiple devices without the
 * need for knowing each device interrupt identifier.
 */
Status merlin_platform_driver_irq_displatch(uint32_t IRQn);

/**
 * @brief configure the driver's target device associted GPIO, when there are some
 *
 * This function use the dts-declared pinmux informations in order to configure the GPIO controller
 * using the Sentry kernel UAPI.
 * Once successfully called, the GPIOs are properly set so that the device can interract with outer world.
 */
Status merlin_platform_driver_configure_gpio(struct platform_device_driver *self);

#endif/*!MERLIN_PLATFORM_H*/
