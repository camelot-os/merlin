// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_USB_H
#define MERLIN_USB_H

#include <types.h>
#include <merlin/platform/driver.h>

/**
 * @brief USB controller maximum-speed values exposed by the DTS backend.
 */
enum usb_maximum_speed {
	USB_MAXIMUM_SPEED_LOW,
	USB_MAXIMUM_SPEED_FULL,
	USB_MAXIMUM_SPEED_HIGH,
	USB_MAXIMUM_SPEED_SUPER,
	USB_MAXIMUM_SPEED_UNKNOWN,
};

enum usb_otg_mode {
    USB_OTG_MODE_HOST,
    USB_OTG_MODE_DEVICE,
    USB_OTG_MODE_UNKNOWN,
};

/** USB driver structure pre-declaration */
struct usb_driver;

/**
 * @brief Set the USB device address on the controller.
 *
 * @param self pointer to the usb_driver structure, which contains the private data and the fops of the driver
 * @param address USB device address to configure
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usb_set_address_fn_t)(struct usb_driver *self, uint8_t address);

/**
 * @brief Write data to a USB endpoint.
 *
 * @param self pointer to the usb_driver structure, which contains the private data and the fops of the driver
 * @param endpoint target endpoint number
 * @param wrbuf buffer containing the payload to transmit
 * @param len number of bytes to write
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usb_write_endpoint_fn_t)(struct usb_driver *self, uint8_t endpoint, const uint8_t *wrbuf, size_t len);

/**
 * @brief Read data from a USB endpoint.
 *
 * @param self pointer to the usb_driver structure, which contains the private data and the fops of the driver
 * @param endpoint source endpoint number
 * @param rdbuf buffer used to store the received payload
 * @param len number of bytes to read
 *
 * @return 0 on success, negative error code on failure
 */
typedef int (*usb_read_endpoint_fn_t)(struct usb_driver *self, uint8_t endpoint, uint8_t *rdbuf, size_t len);

/* USB specific functions that need to be implemented by the driver */

/**
 * @brief USB specific fops definition
 *
 * These functions need to be implemented in the USB driver and declared
 */
struct usb_bus_fops {
	usb_set_address_fn_t set_address;            /**< configure the USB device address */
	usb_write_endpoint_fn_t write_endpoint;      /**< write data to an endpoint */
	usb_read_endpoint_fn_t read_endpoint;        /**< read data from an endpoint */
};

/**
 * @brief declare a USB bus driver, which is a type of platform driver
 */
struct usb_driver {
	struct usb_bus_fops * fops;             /**< USB specific fops, see above */
	struct platform_device * platform_fops; /**< platform-generic fops */
	void *private_data;                     /**< private data for the driver, if needed */
};


#endif/*!MERLIN_USB_H*/
