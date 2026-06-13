// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#include <types.h>
#include <merlin/platform/driver.h>

#include "can_dt.h"
#include "can.h"

Status merlin_platform_dts_can_get_devinfo(uint32_t devlabel, const devinfo_t **devinfo)
{
    Status status = STATUS_NO_ENTITY;

#if DEV_ID_CAN_MAX > 0
    for (size_t i = 0; i < DEV_ID_CAN_MAX; i++) {
        if (can_devices[i].devinfo.id == devlabel) {
            *devinfo = &can_devices[i].devinfo;
            status = STATUS_OK;
        }
    }
#endif

    return status;
}

Status merlin_can_get_precaler_div(struct platform_device_driver *drv, uint32_t *precaler_div)
{
    Status status = STATUS_INVALID;

    if (unlikely(drv == NULL || precaler_div == NULL)) {
        return STATUS_INVALID;
    }

#if DEV_ID_CAN_MAX > 0
    for (size_t i = 0; i < DEV_ID_CAN_MAX; i++) {
        if (drv->label == can_devices[i].devinfo.id) {
            *precaler_div = can_devices[i].bus_input_clock_freq;
            return STATUS_OK;
        }
    }
#endif

    return status;
}
