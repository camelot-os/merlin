// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#include <merlin/platform/driver.h>
#include <merlin/helpers.h>
#include "i2c_dt.h"

Status merlin_platform_driver_get_external_device_parent_bus_label(struct platform_device_driver *drv, uint32_t *bus_label)
{
    Status status = STATUS_NO_ENTITY;
    if (unlikely(drv == NULL || bus_label == NULL)) {
        return STATUS_INVALID;
    }

    /*
     * external bus-connected devices is a child of one of the various active buses
     * note that this requires that both the bus and the external device are properly
     * declared in the dts, with the correct parent-child relationship,
     * and that both are owned by the current task.
     */
#if DEV_ID_I2C_MAX > 0
    for (size_t i = 0; i < DEV_ID_I2C_MAX; i++) {
        // Itérer sur les enfants du bus I2C
        for (size_t j = 0; j < i2c_devices[i].num_child_devices; j++) {
            // Si on trouve une correspondance avec le label du périphérique externe
            if (i2c_devices[i].child_devinfo[j].id == drv->label) {
                // Retourner le label du bus parent
                *bus_label = i2c_devices[i].devinfo.id;
                status = STATUS_OK;
                break;
            }
        }
        if (status == STATUS_OK) {
            break;
        }
    }
#endif
    return status;
}
