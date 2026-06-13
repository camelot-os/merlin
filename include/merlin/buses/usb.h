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

enum usb_otg_mode {
    USB_OTG_MODE_HOST,
    USB_OTG_MODE_DEVICE,
    USB_OTG_MODE_UNKNOWN,
};


#endif/*!MERLIN_USB_H*/
