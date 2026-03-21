// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#ifndef MERLIN_PRIV_USB_H
#define MERLIN_PRIV_USB_H

#include <device.h>
#include <inttypes.h>
#include <merlin/platform/driver.h>
#include <merlin/buses/usb.h>
#include <merlin/helpers.h>

Status merlin_platform_dts_usb_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo);

Status merlin_usb_get_maximum_speed(struct platform_device_driver *drv, enum usb_maximum_speed *maximum_speed);


#endif/* MERLIN_PRIV_USB_H */
