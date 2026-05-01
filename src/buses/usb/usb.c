// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#include <types.h>
#include <merlin/platform/driver.h>
#include "usb_dt.h"
#include "usb.h"



Status merlin_platform_dts_usb_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo)
{
    Status status = STATUS_NO_ENTITY;
#if DEV_ID_USB_MAX > 0
    for (size_t i = 0; i < DEV_ID_USB_MAX; i++) {
        if (usb_devices[i].devinfo.id == devlabel) {
            *devinfo = &usb_devices[i].devinfo;
            status = STATUS_OK;
        }
    }
#endif
    return status;
}

Status merlin_usb_get_maximum_speed(struct platform_device_driver *drv, enum usb_maximum_speed *maximum_speed)
{
    Status status = STATUS_INVALID;
    if (unlikely(drv == NULL || maximum_speed == NULL)) {
        return STATUS_INVALID;
    }
#if DEV_ID_USB_MAX > 0
    for (size_t i = 0; i < DEV_ID_USB_MAX; i++) {
        if (drv->label == usb_devices[i].devinfo.id) {
            *maximum_speed = (enum usb_maximum_speed)usb_devices[i].maximum_speed;
            return STATUS_OK;
        }
    }
#endif
    return status;
}
