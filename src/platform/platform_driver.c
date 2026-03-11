// SPDX-License-Identifier: Apache-2.0
// Copyright (c) 2026 H2Lab Development Team

#include <types.h>
#include <handle.h>
#include <uapi.h>
#include <merlin/platform/driver.h>
#include <merlin/helpers.h>
#include "../buses/i2c/i2c.h"
#include "dts.h"

/* Note that Sentry tasks are monothreaded and thus do not require locks or concurrency check ath Merlin level */
static struct platform_device_driver *registered_drivers[CONFIG_MAX_REGISTERED_DRIVERS];
static size_t num_registered_drivers = 0;

static struct platform_device_driver *merlin_platform_get_from_handle(devh_t handle)
{
    /* define a merlin-level platform drivers listing context to search in */
    return NULL;
}

/* Map given device handle into the task memory */
Status merlin_platform_driver_map(struct platform_device_driver *self)
{
    if (unlikely(self == NULL)) {
        return STATUS_INVALID;
    }
    return __sys_map_dev(self->devh);
}

/* Unmap given device handle from the task memory */
Status merlin_platform_driver_unmap(struct platform_device_driver *self)
{
    if (unlikely(self == NULL)) {
        return STATUS_INVALID;
    }
    return __sys_unmap_dev(self->devh);
}

/* resolve driver context in the registered driver(s) for given IRQn using DTS backend,
 * and call the fops->isr routine for it.
 * Minimalist model, to auto-dispatch IRQ events received from sys_get_event()
 */
Status merlin_platform_driver_irq_displatch(uint32_t IRQn)
{
    struct platform_device_driver platform_device_driver;
    if (unlikely(merlin_platform_dts_get_driver(IRQn, &platform_device_driver) != STATUS_OK)) {
        return STATUS_INVALID;
    }
    struct platform_device_driver *self = merlin_platform_get_from_handle(platform_device_driver.devh);
    return self->platform_fops.isr(self, IRQn);
}


size_t merlin_platform_get_baseaddr(struct platform_device_driver *self)
{
    return merlin_platform_dts_get_baseaddr(self);
}

Status merlin_platform_driver_register(struct platform_device_driver *driver, uint32_t label)
{
    const devinfo_t *devinfo;
    if (unlikely(driver == NULL)) {
        return STATUS_INVALID;
    }
    if (unlikely(num_registered_drivers >= CONFIG_MAX_REGISTERED_DRIVERS)) {
        return STATUS_BUSY;
    }
    switch (driver->type) {
        case DEVICE_TYPE_I2C:
            if (unlikely(merlin_platform_dts_i2c_get_devinfo(label, &devinfo) != STATUS_OK)) {
                return STATUS_INVALID;
            }
            break;
        default:
            return STATUS_INVALID;
    }
    driver->label = label;
    /* FIXME: Sentry kernel device list must be fixed to use sentry,label instead of loop index for identifying devices,
     * This is the way other ressources (such as SHM or DMA) work, and is required for consistent device management at userspace level */

     /* get device handle based on its label */
    if (unlikely(__sys_get_device_handle(label) != STATUS_OK)) {
        return STATUS_INVALID;
    }
    copy_from_kernel((uint8_t*)&driver->devh, sizeof(devh_t));

    registered_drivers[num_registered_drivers] = driver;
    num_registered_drivers++;
    driver->devinfo = devinfo;

    return STATUS_OK;
}

Status merlin_platform_driver_configure_gpio(struct platform_device_driver *self)
{
    Status status = STATUS_INVALID;
    const devinfo_t *devinfo = NULL;
    if (unlikely(self == NULL)) {
        goto end;
    }
    devinfo = self->devinfo;
    if (unlikely(devinfo == NULL)) {
         goto end;
    }
    for (size_t i = 0; i < devinfo->num_ios; i++) {
        status = __sys_gpio_configure(self->devh, i);
        if (unlikely(status != STATUS_OK)) {
            goto end;
        }
    }
end:
    return status;
}
