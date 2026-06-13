// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_USB_H
#define MERLIN_USB_H

#include <types.h>

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

/**
 * @brief USB controller operating mode.
 */
enum usb_otg_mode {
	USB_OTG_MODE_HOST,
	USB_OTG_MODE_DEVICE,
	USB_OTG_MODE_UNKNOWN,
};

/**
 * @brief USB endpoint identifiers.
 */
enum usb_endpoint_number {
	USB_ENDPOINT_0,
	USB_ENDPOINT_1,
	USB_ENDPOINT_2,
	USB_ENDPOINT_3,
	USB_ENDPOINT_4,
	USB_ENDPOINT_5,
	USB_ENDPOINT_6,
	USB_ENDPOINT_7,
	USB_ENDPOINT_UNKNOWN,
};

/**
 * @brief USB endpoint direction values.
 */
enum usb_endpoint_direction {
	USB_ENDPOINT_DIR_IN,
	USB_ENDPOINT_DIR_OUT,
	USB_ENDPOINT_DIR_BOTH,
	USB_ENDPOINT_DIR_UNKNOWN,
};

/**
 * @brief USB endpoint transfer types.
 */
enum usb_endpoint_type {
	USB_ENDPOINT_TYPE_CONTROL,
	USB_ENDPOINT_TYPE_ISOCHRONOUS,
	USB_ENDPOINT_TYPE_BULK,
	USB_ENDPOINT_TYPE_INTERRUPT,
	USB_ENDPOINT_TYPE_UNKNOWN,
};

/**
 * @brief USB endpoint data toggle.
 */
enum usb_endpoint_toggle {
	USB_ENDPOINT_TOGGLE_DATA0,
	USB_ENDPOINT_TOGGLE_DATA1,
};

/**
 * @brief USB endpoint runtime state.
 */
enum usb_endpoint_state {
	USB_ENDPOINT_STATE_IDLE,
	USB_ENDPOINT_STATE_SETUP_WIP,
	USB_ENDPOINT_STATE_SETUP,
	USB_ENDPOINT_STATE_STATUS,
	USB_ENDPOINT_STATE_STALL,
	USB_ENDPOINT_STATE_DATA_IN_WIP,
	USB_ENDPOINT_STATE_DATA_IN,
	USB_ENDPOINT_STATE_DATA_OUT_WIP,
	USB_ENDPOINT_STATE_DATA_OUT,
	USB_ENDPOINT_STATE_INVALID,
};

/**
 * @brief USB port speed as seen by the device controller.
 */
enum usb_port_speed {
	USB_PORT_SPEED_LOW,
	USB_PORT_SPEED_FULL,
	USB_PORT_SPEED_HIGH,
	USB_PORT_SPEED_UNKNOWN,
};

/**
 * @brief USB endpoint 0 maximum packet size encodings.
 */
enum usb_endpoint0_mpsize {
	USB_EP0_MPSIZE_64BYTES = 0,
	USB_EP0_MPSIZE_32BYTES = 1,
	USB_EP0_MPSIZE_16BYTES = 2,
	USB_EP0_MPSIZE_8BYTES = 3,
};

/**
 * @brief USB non-control endpoint maximum packet size values.
 */
enum usb_endpoint_mpsize {
	USB_ENDPOINT_MPSIZE_64BYTES = 64,
	USB_ENDPOINT_MPSIZE_128BYTES = 128,
	USB_ENDPOINT_MPSIZE_512BYTES = 512,
	USB_ENDPOINT_MPSIZE_1024BYTES = 1024,
};

#endif /*!MERLIN_USB_H*/
